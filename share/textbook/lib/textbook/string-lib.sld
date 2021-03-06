;; Copyright (c) 2017 by Gregor Klinke
;; All rights reserved.

(define-library (textbook string-lib)
  (import (chibi))
  (export whitespace?
          string-trim string-trim-right string-trim-both
          string-pad-left
          string-upcase-ascii string-downcase-ascii
;;          string-prefix?
          string-join string-split string-find string-find-right
          string-contains?
          string-empty?
          )
  (include "string-lib.scm")
  )
