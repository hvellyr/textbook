// Copyright (c) 2015 Gregor Klinke
// All rights reserved.

#include "tex-processor.hpp"

#include "estd/memory.hpp"
#include "fo-processor.hpp"
#include "fo.hpp"
#include "fos.hpp"
#include "sosofo.hpp"
#include "utils.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/optional/optional.hpp>

#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_set>
#include <sstream>
#include <string>

namespace eyestep {

namespace fs = boost::filesystem;
namespace po = boost::program_options;


//------------------------------------------------------------------------------

namespace {
  const std::string k_pt = "pt";
  const std::string k_px = "px";
  const std::string k_em = "em";
  const std::string k_m = "m";
  const std::string k_mm = "mm";
  const std::string k_cm = "cm";
  const std::string k_in = "in";

  std::string unit_name(fo::Unit un)
  {
    switch (un) {
    case fo::k_pt:
      return k_pt;
    case fo::k_px:
      return k_px;
    case fo::k_m:
      return k_m;
    case fo::k_mm:
      return k_mm;
    case fo::k_cm:
      return k_cm;
    case fo::k_em:
      return k_em;
    case fo::k_in:
      return k_in;
    }
  }


  std::string dimen2str(const fo::LengthSpec& ls)
  {
    double max_inf = std::numeric_limits<double>::infinity();

    std::stringstream ss;
    ss << ls._value << unit_name(ls._unit);

    if (ls._max == max_inf) {
      if (ls._priority > 1)
        ss << " plus 1fill";
      else
        ss << " plus 1fil";
    }
    else if (ls._max != ls._value)
      ss << " plus " << ls._max << unit_name(ls._unit);

    if (ls._min != ls._value)
      ss << " minus " << ls._min << unit_name(ls._unit);

    return ss.str();
  }


  std::string exact_dimen2str(const fo::LengthSpec& ls)
  {
    std::stringstream ss;
    ss << ls._value << unit_name(ls._unit);
    return ss.str();
  }


  std::string max_dimen2str(const fo::LengthSpec& ls)
  {
    double max_inf = std::numeric_limits<double>::infinity();

    std::stringstream ss;
    if (ls._max == max_inf)
      ss << "\\hfill{}";
    else
      ss << ls._max << unit_name(ls._unit);
    return ss.str();
  }


  void escape_str_to_stream(std::ostream& os, const std::string& str,
                            tex_detail::TexStyleContext& ctx)
  {
    std::u16string str16 = utils::utf8_to_u16string(str);

    for (const auto c : str16) {
      switch (c) {
      case '\\':
        os << "\\textbackslash{}";
        break;
      case '{':
        os << "\\textbraceleft{}";
        break;
      case '}':
        os << "\\textbraceright{}";
        break;
      case '%':
        os << "\\%";
        break;
      case '&':
        os << "\\&";
        break;
      case '<':
        os << "\\tbless{}";
        break;
      case '>':
        os << "\\tbgreater{}";
        break;
      case '$':
        os << "\\textdollar{}";
        break;
      case '_':
        os << "\\textunderscore{}";
        break;
      case '#':
        os << "\\#";
        break;
      case '^':
        os << "\\textasciicircum{}";
        break;
      case '~':
        os << "\\textasciitilde{}";
        break;
      case '|':
        os << "{\\ttfamily|}";
        break;
      case 0x3008:
        os << "$\\langle$";
        break;
      case 0x3009:
        os << "$\\rangle$";
        break;
      case 0x2022:
        os << "\\textbullet{}";
        break;

      case 0x2013: os << "--"; break;
      case 0x201c: os << "\\ldqo{}"; break;
      case 0x201d: os << "\\rdqo{}"; break;
      case 0x2018: os << "\\lsqo{}"; break;
      case 0x2019: os << "\\rsqo{}"; break;
      case 0x2026: os << "\\hellip{}"; break;

      case 0x21d2: os << "$\\Rightarrow{}$"; break;
      case 0x21a6: os << "$\\mapsto{}$"; break;
      case 0x2261: os << "$\\equiv{}$"; break;
      case 0x22a3: os << "$\\dashv{}$"; break;

      case '\n':
        switch (ctx._wrapstyle) {
        case tex_detail::k_asis_wrap:
          os << "\\\\\\relax" << std::endl;
          break;
        case tex_detail::k_normal_wrap:
          os << " ";
          break;
        case tex_detail::k_no_wrap:
          os << " ";
          break;
        }
        break;

      case ' ':
      case '\t':
        switch (ctx._wstreatment) {
        case tex_detail::k_preserve_ws:
          os << "\\ws{}";
          break;
        case tex_detail::k_collapse_ws:
          os << ' ';
          break;
        case tex_detail::k_ignore_ws:
          break;
        }
        break;

      case '\r':
        break;

      default:
        os << utils::u16string_to_utf8(std::u16string() + c);
      }
    }
  }


  void vspace(TexProcessor* po, const IFormattingObject* fo,
              const char* property_name, const char* keep_prop_name)
  {
    auto nobreak = po->property_or_none<bool>(fo, keep_prop_name);
    auto val = po->property_or_none<fo::LengthSpec>(fo, property_name);

    if (val && val->_value != 0) {
      if (nobreak && *nobreak)
        po->stream() << "\\nobreak" << std::endl;
      if (val->_conditionalp)
        po->stream() << "\\vsc{" << dimen2str(*val) << "}";
      else
        po->stream() << "\\vs{" << dimen2str(*val) << "}";
      if (nobreak && *nobreak)
        po->stream() << "\\nobreak" << std::endl;
    }
    else if (nobreak && *nobreak)
      po->stream() << "\\nobreak" << std::endl;
  }


  void enc_fontsize(TexProcessor* po, const IFormattingObject* fo)
  {
    auto fs = po->property(fo, "font-size", fo::LengthSpec(fo::kDimen, 10.0, fo::k_pt));
    auto ls = po->property(fo, "line-spacing",
                           fo::LengthSpec(fo::kDimen, fs._value * 1.2, fo::k_pt));
    // auto posptshift = po->property(fo, "position-point-shift",
    //                                fo::LengthSpec(fo::kDimen, 0.0, fo::k_pt));
    auto fontcaps = po->property(fo, "font-caps", std::string("normal"));

    if (fontcaps == "normal")
      po->style_ctx()._capsstyle = tex_detail::k_normal_caps;
    else if (fontcaps == "lower")
      po->style_ctx()._capsstyle = tex_detail::k_lower_caps;
    else if (fontcaps == "caps")
      po->style_ctx()._capsstyle = tex_detail::k_upper_caps;
    else if (fontcaps == "small-caps")
      po->style_ctx()._capsstyle = tex_detail::k_small_caps;

    po->stream() << "\\fns{" << dimen2str(fs)
                 << "}{" << dimen2str(ls) << "}";

    if (auto fw = po->property_or_none<std::string>(fo, "font-weight")) {
      if (*fw == "bold")
        po->stream() << "\\bfseries{}";
      else if (*fw == "medium")
        po->stream() << "\\mdseries{}";
    }

    if (auto fp = po->property_or_none<std::string>(fo, "font-posture")) {
      if (*fp == "italic")
        po->stream() << "\\itshape{}";
      else if (*fp == "oblique")
        po->stream() << "\\slshape{}";
      else if (*fp == "upright")
        po->stream() << "\\upshape{}";
    }

    if (auto fn = po->property_or_none<std::string>(fo, "font-name")) {
      if (*fn == "monospace")
        po->stream() << "\\ttfamily{}";
      else if (*fn == "sans-serif")
        po->stream() << "\\sffamily{}";
      else if (*fn == "roman")
        po->stream() << "\\rmfamily{}";
    }
  }


  void enc_paraprops(TexProcessor* po, const IFormattingObject* fo)
  {
    double max_inf = std::numeric_limits<double>::infinity();

    auto ind = dimen2str(po->property(fo, "first-line-start-indent",
                                      fo::LengthSpec(fo::kInline, 0.0, fo::k_pt)));
    auto ext = dimen2str(po->property(fo, "last-line-end-indent",
                                      fo::LengthSpec(fo::kInline, 1.0, fo::k_em, 1.0, max_inf)));
    auto sind = dimen2str(po->property(fo, "start-indent",
                                       fo::LengthSpec(fo::kInline, 0.0, fo::k_pt)));
    auto eind = dimen2str(po->property(fo, "end-indent",
                                       fo::LengthSpec(fo::kInline, 0.0, fo::k_pt)));

    po->stream() << "\\ps{" << ind << "}{" << ext << "}{" << sind << "}{"
                 << eind << "}";
  }


  void enc_color(TexProcessor* po, const fo::Color co)
  {
    switch (co._space) {
    case fo::kRGB:
      po->stream() << "\\color[rgb]{"
                   << co._rgb._red << ","
                   << co._rgb._green << ","
                   << co._rgb._blue << "}";
      break;
    case fo::kCMYK:
      po->stream() << "\\color[cmyk]{"
                   << co._cmyk._cyan << ","
                   << co._cmyk._magenta << ","
                   << co._cmyk._yellow << ","
                   << co._cmyk._black << "}";
      break;
    case fo::kGray:
      po->stream() << "\\color[gray]{"
                   << co._gray << "}";
      break;
    }
  }


  class TexLiteralFoProcessor : public IFoProcessor<TexProcessor> {
  public:
    void render(TexProcessor* po, const IFormattingObject* fo) const override
    {
      po->finalize_breaks();
      auto str = static_cast<const fo::Literal*>(fo)->text();

      switch (po->style_ctx()._capsstyle) {
      case tex_detail::k_normal_caps:
        escape_str_to_stream(po->stream(), str, po->style_ctx());
        break;
      case tex_detail::k_lower_caps:
        escape_str_to_stream(po->stream(), boost::to_lower_copy(str),
                             po->style_ctx());
        break;
      case tex_detail::k_upper_caps:
        escape_str_to_stream(po->stream(), boost::to_upper_copy(str),
                             po->style_ctx());
        break;
      case tex_detail::k_small_caps:
        po->stream() << "{\\sc ";
        escape_str_to_stream(po->stream(), str, po->style_ctx());
        po->stream() << "}";
        break;
      default:
        assert(false);
      }
    }
  };


  class TexParagraphFoProcessor : public IFoProcessor<TexProcessor> {
  public:
    void render(TexProcessor* po, const IFormattingObject* fo) const override
    {
      po->finalize_breaks();

      auto lastctx = po->style_ctx();

      vspace(po, fo, "space-before", "keep-with-previous?");

      auto debug_info = po->property_or_none<std::string>(fo, "metadata.debug-info");
      if (debug_info)
        po->stream() << "%%debug: " << *debug_info << std::endl;

      po->stream() << "\\para{";
      enc_paraprops(po, fo);

      auto linesprops = po->property(fo, "lines", std::string("wrap"));
      if (linesprops == "asis")
        po->style_ctx()._wrapstyle = tex_detail::k_asis_wrap;

      auto wstreatment = po->property(fo, "whitespace-treatment",
                                      std::string("collapse"));
      if (wstreatment == "preserve")
        po->style_ctx()._wstreatment = tex_detail::k_preserve_ws;
      else if (wstreatment == "collapse")
        po->style_ctx()._wstreatment = tex_detail::k_collapse_ws;
      else if (wstreatment == "ignore")
        po->style_ctx()._wstreatment = tex_detail::k_ignore_ws;

      po->stream() << "}{";
      enc_fontsize(po, fo);

      auto quadding = po->property(fo, "quadding", std::string("left"));
      if (quadding == "left")
        po->stream() << "\\quadleft{}";
      else if (quadding == "right")
        po->stream() << "\\quadright{}";
      else if (quadding == "center")
        po->stream() << "\\quadcenter{}";

      po->render_sosofo(&fo->port("text"));

      if (wstreatment == "preserve") {
        po->style_ctx()._wstreatment = tex_detail::k_preserve_ws;
        po->stream() << "\\vspace{-1\\baselineskip}";
      }
      else if (wstreatment == "collapse")
        po->style_ctx()._wstreatment = tex_detail::k_collapse_ws;
      else if (wstreatment == "ignore")
        po->style_ctx()._wstreatment = tex_detail::k_ignore_ws;

      po->stream() << "}";
      if (debug_info)
        po->stream() << "%%debug: " << *debug_info << std::endl;

      vspace(po, fo, "space-after", "keep-with-next?");

      po->style_ctx() = lastctx;

      po->stream() << std::endl;
    }
  };


  class TexParagraphBreakFoProcessor : public IFoProcessor<TexProcessor> {
  public:
    void render(TexProcessor* po, const IFormattingObject* fo) const override
    {
      po->finalize_breaks();
      po->stream() << std::endl
                   << "\\newline{}"
                   << std::endl;
    }
  };


  class TexDisplayGroupFoProcessor : public IFoProcessor<TexProcessor> {
  public:
    void render(TexProcessor* po, const IFormattingObject* fo) const override
    {
      if (auto break_before = po->property_or_none<std::string>(fo, "break-before?")) {
        if (*break_before == "page")
          po->request_page_break(tex_detail::kBreakPageBefore);
      }

      vspace(po, fo, "space-before", "keep-with-previous?");

      auto& text_port = fo->port("text");
      if (text_port.length() > 0) {
        po->render_sosofo(&text_port);
      }
      else {
        po->finalize_breaks();
      }

      vspace(po, fo, "space-after", "keep-with-next?");

      if (auto break_after = po->property_or_none<std::string>(fo, "break-after?")) {
        if (*break_after == "page")
          po->request_page_break(tex_detail::kBreakPageAfter);
      }
    }
  };


  class TexSimplePageSequenceFoProcessor : public IFoProcessor<TexProcessor> {
  public:
    void render(TexProcessor* po, const IFormattingObject* fo) const override
    {
      if (!po->_first_page) {
        po->stream() << std::endl
                     << "\\newpage{}" << std::endl
                     << std::endl;
        po->request_page_break(tex_detail::kBreakPageBefore);
      }
      else {
        po->_first_page = false;
      }
      po->request_page_break(tex_detail::kNoBreak);

      auto title = po->property(fo, "metadata.title", std::string());
      auto author = po->property(fo, "metadata.author", std::string());
      auto desc = po->property(fo, "metadata.desc", std::string());
      auto cover = po->property(fo, "metadata.cover", std::string());

      auto pagewidth = po->property(fo, "page-width",
                                    fo::LengthSpec(fo::kDimen, 210, fo::k_mm));
      auto pageheight = po->property(fo, "page-height",
                                     fo::LengthSpec(fo::kDimen, 297, fo::k_mm));
      auto leftmargin = po->property(fo, "left-margin",
                                     fo::LengthSpec(fo::kDimen, 20, fo::k_mm));
      auto rightmargin = po->property(fo, "right-margin",
                                      fo::LengthSpec(fo::kDimen, 20, fo::k_mm));
      auto topmargin = po->property(fo, "top-margin",
                                    fo::LengthSpec(fo::kDimen, 20, fo::k_mm));
      auto bottommargin = po->property(fo, "bottom-margin",
                                       fo::LengthSpec(fo::kDimen, 30, fo::k_mm));
      auto headermargin = po->property(fo, "header-margin",
                                       fo::LengthSpec(fo::kDimen, 10, fo::k_mm));
      // auto footermargin = po->property(fo, "footer-margin",
      //                                  fo::LengthSpec(fo::kDimen, 20, fo::k_mm));
      auto startmargin = po->property(fo, "start-margin",
                                      fo::LengthSpec(fo::kDimen, 0, fo::k_pt));
      po->_current_start_margin = startmargin;
      auto endmargin = po->property(fo, "end-margin",
                                    fo::LengthSpec(fo::kDimen, 0, fo::k_pt));

      po->stream() << "\\setlength\\paperwidth{" << dimen2str(pagewidth)
                   << "}" << std::endl;
      po->stream() << "\\setlength\\paperheight{" << dimen2str(pageheight)
                   << "}" << std::endl;

      po->stream() << "\\setlength\\textwidth{\\paperwidth}" << std::endl;
      po->stream() << "\\setlength\\oddsidemargin{" << dimen2str(leftmargin)
                   << "}" << std::endl
                   << "\\setlength\\evensidemargin{" << dimen2str(leftmargin)
                   << "}" << std::endl
                   << "\\addtolength{\\textwidth}{-" << dimen2str(leftmargin)
                   << "}" << std::endl;

      po->stream()
        << "\\addtolength{\\textwidth}{-" << dimen2str(rightmargin) << "}" << std::endl;

      po->stream()
        << "\\setlength{\\hoffset}{210mm}" << std::endl
        << "\\addtolength{\\hoffset}{-" << dimen2str(pagewidth) << "}" << std::endl
        << "\\divide\\hoffset by 2" << std::endl
        // to correct tex's auto hoffset of 1in
        << "\\addtolength{\\hoffset}{-1in}" << std::endl;

      po->stream()
        << "\\setlength\\textheight{\\paperheight}" << std::endl
        << "\\addtolength{\\textheight}{-" << dimen2str(topmargin) << "}" << std::endl
        << "\\addtolength{\\textheight}{-" << dimen2str(bottommargin) << "}" << std::endl;

      // dsssl:topmargin = \voffset + \topmargin + \headheight + \headersep
      // dsssl:headermargin = \voffset + \topmargin + \headheight
      // -> dsssl:topmargin = dsssl:headermargin + \headersep
      // -> \headersep = dsssl:topmargin - dsssl:headermargin
      //
      // \topmargin = dsssl:topmargin - \headheigth - \headersep
      // \headheight => ? ;; can't be computed from the dsssl
      // characteristics.  Let's assume 12pt (=1\baselineskip)
      po->stream()
        << "\\setlength\\headsep{" << dimen2str(topmargin) << "}" << std::endl
        << "\\addtolength\\headsep{-" << dimen2str(headermargin) << "}" << std::endl
        << "\\setlength\\headheight{12pt}" << std::endl
        << "\\setlength\\topmargin{" << dimen2str(topmargin) << "}" << std::endl
        << "\\addtolength\\topmargin{-\\headsep}" << std::endl
        << "\\addtolength\\topmargin{-\\headheight}" << std::endl;

      // let's center the page on the paper
      // \voffset = dsssl:pageheight / 2
      po->stream()
        << "\\setlength\\voffset{297mm}" << std::endl
        << "\\addtolength{\\voffset}{-" << dimen2str(pageheight) << "}" << std::endl
        << "\\divide\\voffset by 2" << std::endl
        // to correct tex's auto voffset of 1in
        << "\\addtolength{\\voffset}{-1in}" << std::endl;

      po->stream()
        << "\\setlength{\\leftskip}{" << dimen2str(startmargin) << "}" << std::endl
        << "\\setlength{\\rightskip}{" << dimen2str(endmargin) << "}" << std::endl;

      po->stream()
        << "\\changelayout%%" << std::endl;

      //--------
      auto leftheader =
        po->property_or_none<std::shared_ptr<Sosofo>>(fo, "left-header");
      auto centerheader =
        po->property_or_none<std::shared_ptr<Sosofo>>(fo, "center-header");
      auto rightheader =
        po->property_or_none<std::shared_ptr<Sosofo>>(fo, "right-header");

      if (leftheader || rightheader || centerheader) {
        po->stream() << "\\makeheadline{";
        if (leftheader)
          po->render_sosofo(leftheader->get());
        po->stream() << "}{";
        if (centerheader)
          po->render_sosofo(centerheader->get());
        po->stream() << "}{";
        if (rightheader)
          po->render_sosofo(rightheader->get());
        po->stream() << "}";
      }

      auto leftfooter =
        po->property_or_none<std::shared_ptr<Sosofo>>(fo, "left-footer");
      auto centerfooter =
        po->property_or_none<std::shared_ptr<Sosofo>>(fo, "center-footer");
      auto rightfooter =
        po->property_or_none<std::shared_ptr<Sosofo>>(fo, "right-footer");

      if (leftfooter || rightfooter || centerfooter) {
        po->stream() << "\\makefootline{";
        if (leftfooter)
          po->render_sosofo(leftfooter->get());
        po->stream() << "}{";
        if (centerfooter)
          po->render_sosofo(centerfooter->get());
        po->stream() << "}{";
        if (rightfooter)
          po->render_sosofo(rightfooter->get());
        po->stream() << "}";
      }

      po->stream() << "\\hsize=\\textwidth" << std::endl;

      po->stream() << "\\def\\fnlayout{%%" << std::endl;
      po->stream() << "\\setlength{\\leftskip}{" << dimen2str(startmargin)
                   << "}%%" << std::endl;
      po->stream() << "\\setlength{\\rightskip}{" << dimen2str(endmargin)
                   << "}}%%" << std::endl;
      // an anchor to make vspaces on start pages possible
      po->stream() << "\\hbox{}\\par\\vs{-2\\baselineskip}%%" << std::endl;

      po->request_page_break(tex_detail::kSurpressNextPageBreak);
      po->render_sosofo(&fo->port("text"));
    }
  };


  class TexSequenceFoProcessor : public IFoProcessor<TexProcessor> {
  public:
    void render(TexProcessor* po, const IFormattingObject* fo) const override
    {
      po->finalize_breaks();
      po->stream() << "{";

      auto pps = po->property_or_none<fo::LengthSpec>(fo, "position-point-shift");

      if (pps) {
        if (pps->_value > 0)
          po->stream() << "\\h{";
        else if (pps->_value < 0)
          po->stream() << "\\t{";
      }

      enc_fontsize(po, fo);
      if (auto co = po->property_or_none<fo::Color>(fo, "color"))
        enc_color(po, *co);
      po->render_sosofo(&fo->port("text"));

      if (pps) {
        if (pps->_value > 0)
          po->stream() << "}";
        else if (pps->_value < 0)
          po->stream() << "}";
      }

      po->stream() << "}";
    }
  };


  const std::string k_left = "left";
  const std::string k_center = "center";
  const std::string k_right = "right";

  class TexLineFieldFoProcessor : public IFoProcessor<TexProcessor> {
  public:
    void render(TexProcessor* po, const IFormattingObject* fo) const override
    {
      po->finalize_breaks();

      auto field_width =
        po->property_or_none<fo::LengthSpec>(fo, "field-width");
      auto field_align = po->property_or_none<std::string>(fo, "field-align");

      if (field_width && field_width->_value > 0) {
        if (field_width->_max != field_width->_value) {
          po->stream() << "{";
          enc_fontsize(po, fo);

          auto left_hss =
            field_align && (*field_align == "right" || *field_align == "center")
              ? "\\hfill"
              : "";
          auto right_hss =
            field_align && (*field_align == "left" || *field_align == "center")
              ? "\\hfill"
              : "";
          po->stream() << "\\lf{" << exact_dimen2str(*field_width) << "}{"
                       << max_dimen2str(*field_width) << "}{" << left_hss
                       << "}{" << right_hss << "}{";
          po->render_sosofo(&fo->port("text"));

          po->stream() << "}}";
          return;
        }
        else
          po->stream() << "\\hbox to " << exact_dimen2str(*field_width) << "{";
      }
      else
        po->stream() << "{";

      if (field_align && (*field_align == "right" || *field_align == "center"))
        po->stream() << "\\hfill{}";

      enc_fontsize(po, fo);
      po->render_sosofo(&fo->port("text"));

      if (field_align && (*field_align == "left" || *field_align == "center"))
        po->stream() << "\\hfill{}";
      po->stream() << "}";
    }
  };


  class TexPageNumberFoProcessor : public IFoProcessor<TexProcessor> {
  public:
    void render(TexProcessor* po, const IFormattingObject* fo) const override
    {
      po->finalize_breaks();

      auto refid = po->property(fo, "refid", std::string("#current"));
      if (refid == "#current")
        po->stream() << "\\pageno{}";
      else if (!refid.empty())
        po->stream() << "\\pageref{" << refid << "}";
    }
  };


  class TexAnchorFoProcessor : public IFoProcessor<TexProcessor> {
  public:
    void render(TexProcessor* po, const IFormattingObject* fo) const override
    {
      po->finalize_breaks();

      if (auto id = po->property_or_none<std::string>(fo, "id")) {
        if (!id->empty())
          po->stream() << "\\label{" << *id << "}";
      }
    }
  };

} // ns anon


TexProcessor::TexProcessor()
  : _verbose(false),
    _style_ctx{tex_detail::k_normal_caps, tex_detail::k_normal_wrap,
               tex_detail::k_collapse_ws}
{
}


TexProcessor::TexProcessor(const po::variables_map& args)
  : TexProcessor()
{
  if (!args.empty()) {
    _verbose = args["verbose"].as<bool>();
  }
}


std::string TexProcessor::proc_id() const
{
  return "tex";
}


std::string TexProcessor::default_output_extension() const
{
  return ".tex";
}

po::options_description TexProcessor::program_options() const
{
  std::string opts_title =
    std::string("Tex renderer [selector: '") + proc_id() + "']";
  po::options_description desc(opts_title);

  return desc;
}

const IFoProcessor<TexProcessor>*
TexProcessor::lookup_fo_processor(const std::string& fo_classname) const
{
  static auto procs =
    std::map<std::string, std::shared_ptr<IFoProcessor<TexProcessor>>>{
      {"#literal", std::make_shared<TexLiteralFoProcessor>()},
      {"#paragraph", std::make_shared<TexParagraphFoProcessor>()},
      {"#paragraph-break", std::make_shared<TexParagraphBreakFoProcessor>()},
      {"#display-group", std::make_shared<TexDisplayGroupFoProcessor>()},
      {"#simple-page-sequence",
       std::make_shared<TexSimplePageSequenceFoProcessor>()},
      {"#sequence", std::make_shared<TexSequenceFoProcessor>()},
      {"#line-field", std::make_shared<TexLineFieldFoProcessor>()},
      {"#page-number", std::make_shared<TexPageNumberFoProcessor>()},
      {"#anchor", std::make_shared<TexAnchorFoProcessor>()}
    };

  auto i_find = procs.find(fo_classname);

  return i_find != procs.end() ? i_find->second.get() : nullptr;
}


void TexProcessor::before_rendering()
{
  _stream.open(_output_file.string(), std::ios_base::out |
                                        std::ios_base::binary |
                                        std::ios_base::trunc);
  if (_stream.fail()) {
    std::cerr << "Opening file '" << _output_file << "' failed" << std::endl;
  }

  //_stream << "\\usepackage{listings}" << std::endl
  _stream << "\\documentclass[a4paper]{textbook}" << std::endl
          << "\\usepackage[utf8]{inputenc}" << std::endl
          << "\\usepackage{makeidx}" << std::endl
          << "\\makeindex" << std::endl;

  _stream << "\\begin{document}" << std::endl;
}


void TexProcessor::after_rendering()
{
  _stream << "\\end{document}" << std::endl;
}


boost::filesystem::ofstream& TexProcessor::stream()
{
  return _stream;
}

tex_detail::TexStyleContext& TexProcessor::style_ctx()
{
  return _style_ctx;
}


void TexProcessor::finalize_breaks()
{
  switch (_break_pending) {
  case tex_detail::kSurpressNextPageBreak:
  case tex_detail::kNoBreak:
    _break_pending = tex_detail::kNoBreak;
    break;
  case tex_detail::kBreakPageBefore:
  case tex_detail::kBreakPageAfter:
    stream() << std::endl
             << "\\newpage{}" << std::endl
             << "\\hbox{}\\par\\vs{-2\\baselineskip}%%" << std::endl
             << std::endl;
    _break_pending = tex_detail::kNoBreak;
    break;
  }
}

void TexProcessor::request_page_break(tex_detail::BreakKind breakKind)
{
  if (breakKind != _break_pending &&
      _break_pending != tex_detail::kSurpressNextPageBreak) {
    _break_pending = breakKind;
  }
}

} // ns eyestep
