(define-library (srfi 89)
  (export $hash-keyword $opt-key $perfect-hash-table-lookup $process-keys
   $req-key $undefined define* lambda* )
  (import (chibi) (scheme cxr) (srfi 88) (sairy macros))
  (include "89/opt-params.scm"))