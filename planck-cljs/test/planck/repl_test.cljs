(ns planck.repl-test
  (:require-macros [planck.repl])
  (:require [cljs.test :refer-macros [deftest testing is]]
            [planck.repl :as repl]))

(deftest get-highlight-coords
  (testing "Highlight coordinates"
    (is (= [0 0] (js->clj (planck.repl/get-highlight-coords 1 "[]" []))))
    (is (= [0 1] (js->clj (planck.repl/get-highlight-coords 2 "[[]]" []))))
    (is (= [0 0] (js->clj (planck.repl/get-highlight-coords 3 "[()]" []))))
    (is (= [0 1] (js->clj (planck.repl/get-highlight-coords 2 " []" []))))
    (is (= [1 0] (js->clj (planck.repl/get-highlight-coords 1 "]" ["["]))))
    (is (= [2 0] (js->clj (planck.repl/get-highlight-coords 1 "]" ["[" ""]))))
    (is (= [1 0] (js->clj (planck.repl/get-highlight-coords 1 "]" ["[" "["]))))
    (is (= [2 0] (js->clj (planck.repl/get-highlight-coords 1 "]" ["[" "[]"]))))
    (is (= [0 1] (js->clj (planck.repl/get-highlight-coords 2 "#{}" []))))
    (is (= [0 0] (js->clj (planck.repl/get-highlight-coords 4 "[\"[\"]" []))))))

(deftest test-apropos
  (is (= '(cljs.core/ffirst) (planck.repl/apropos "ffirst")))
  (is (= '(cljs.core/ffirst) (planck.repl/apropos ffirst)))
  (is (= '(cljs.core/ffirst cljs.core/nfirst) (planck.repl/apropos #"[a-z]+first"))))

(deftest test-dir-planck-repl
  (is (= "*pprint-results*\napropos\ndir\ndoc\nfind-doc\npst\nsource\n" (with-out-str (planck.repl/dir planck.repl)))))

(deftest get-error-indicator-test
  (is (= "             ⬆"
        (repl/get-error-column-indicator
          (ex-info repl/could-not-eval-expr {}
            (ex-info "" {:tag    :cljs/analysis-error
                         :column 3}))
          "foo.core"))))
