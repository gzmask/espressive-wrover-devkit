#!/usr/bin/env bb
(ns test-ota-upload
  {:clj-kondo/config '{:linters {:unresolved-namespace {:exclude [ota-upload]}}}})
(require '[babashka.fs :as fs] '[cheshire.core :as json] '[clojure.string :as str])
(load-file "scripts/ota_upload.bb")
(def root (str ".local/evidence/ota/client-tests-" (random-uuid)))
(fs/create-dirs root {:posix-file-permissions "rwx------"})
(def payload (byte-array 320))
(def footer (ota-upload/sha payload))
(def image (byte-array (concat payload footer)))
(defn save-bytes [name bytes]
  (let [path (fs/path root name)]
    (fs/create-file path {:posix-file-permissions "rw-------"})
    (java.nio.file.Files/write path bytes (make-array java.nio.file.OpenOption 0))
    (str path)))
(def image-path (save-bytes "test.bin" image))
(def token-path (save-bytes "fake-token.bin" (byte-array 32)))
(def cert-path (save-bytes "fake-cert.pem" (.getBytes "not a real certificate" "US-ASCII")))
(def hash-hex (ota-upload/bytes-hex (ota-upload/sha image)))
(def expected-hash (ota-upload/bytes-hex footer))
(defn run-case [name preconfirmed post-code]
  (let [ev (str (fs/path root name))
        reads (atom 0) posts (atom [])
        args ["--ip" "127.0.0.1" "--image" image-path "--cert" cert-path "--token" token-path
              "--sha256" hash-hex "--slot-address" "0x160000" "--evidence-dir" ev]
        failed (atom false)]
    (with-redefs [ota-upload/status!
                  (fn [_ _ _]
                    (let [n (swap! reads inc)]
                      (if (= n 1)
                        {:slot_address 65536 :online true :healthy true :confirmed preconfirmed}
                        {:slot_address 1441792 :image_sha256 expected-hash :version "test"
                         :online true :healthy true :confirmed true})))
                  ota-upload/curl!
                  (fn [cfg _ _ argv]
                    (assert (= 1 (count (filter #{"--url"} argv))))
                    (assert (= 1 (count (filter #{"POST"} argv))))
                    (assert (.contains (slurp cfg) "Authorization: Bearer "))
                    (assert (not-any? #(str/includes? % "Bearer") argv))
                    (swap! posts conj argv)
                    {:code post-code :ok (= "200" post-code)})]
      (try (ota-upload/main args) (catch Exception _ (reset! failed true))))
    (assert (not (fs/exists? (fs/path ev "request.curl"))))
    (cond
      (not preconfirmed) (do (assert @failed) (assert (empty? @posts)) (assert (= 1 @reads)))
      (= post-code "401") (do (assert @failed) (assert (= 1 (count @posts))) (assert (= 1 @reads)))
      :else (do (assert (not @failed)) (assert (= 1 (count @posts))) (assert (= 2 @reads))
                (assert (:pass (json/parse-string (slurp (str (fs/path ev "result.json"))) true)))))))
(run-case "normal" true "200")
(run-case "lost-ack" true "000")
(run-case "unconfirmed" false "200")
(run-case "unauthorized" true "401")
(println "Uploader4mockscenarios PASS; no real network/device calls.")
