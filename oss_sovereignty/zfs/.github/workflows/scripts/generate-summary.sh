FUNCTIONAL_PARTS="4"
ZTS_REPORT="tests/test-runner/bin/zts-report.py"
chmod +x $ZTS_REPORT
function output() {
  echo -e $* >> Summary.md
}
function error() {
  output ":bangbang: $* :bangbang:\n"
}
function generate() {
  test ! -s log && return
  cat log | grep '^Test' > list
  awk '/\[FAIL\]|\[KILLED\]/{ show=1; print; next; }
    /\[SKIP\]|\[PASS\]/{ show=0; } show' log > err
  if [ -s err ]; then
    output "<pre>"
    $ZTS_REPORT --no-maybes ./list >> Summary.md
    output "</pre>"
    ERRLOGS=$((ERRLOGS+1))
    errfile="err-$ERRLOGS.md"
    echo -e "\n## $headline (debugging)\n" >> $errfile
    echo "<details><summary>Error Listing - with dmesg and dbgmsg</summary><pre>" >> $errfile
    dd if=err bs=999k count=1 >> $errfile
    echo "</pre></details>" >> $errfile
  else
    output "All tests passed :thumbsup:"
  fi
  output "<details><summary>Full Listing</summary><pre>"
  cat list >> Summary.md
  output "</pre></details>"
  rm -f err list log
}
function check_tarfile() {
  if [ -f "$1" ]; then
    tar xf "$1" || error "Tarfile $1 returns some error"
  else
    error "Tarfile $1 not found"
  fi
}
function check_logfile() {
  if [ -f "$1" ]; then
    cat "$1" >> log
  else
    error "Logfile $1 not found"
  fi
}
function summarize_s() {
  headline="$1"
  output "\n## $headline\n"
  rm -rf testfiles
  check_tarfile "$2/sanity.tar"
  check_logfile "testfiles/log"
  generate
}
function summarize_f() {
  headline="$1"
  output "\n## $headline\n"
  rm -rf testfiles
  for i in $(seq 1 $FUNCTIONAL_PARTS); do
    tarfile="$2/part$i.tar"
    check_tarfile "$tarfile"
    check_logfile "testfiles/log"
  done
  generate
}
ERRLOGS=0
if [ ! -f Summary/Summary.md ]; then
  echo -n > Summary.md
  summarize_s "Sanity Tests Ubuntu 20.04" Logs-20.04-sanity
  summarize_s "Sanity Tests Ubuntu 22.04" Logs-22.04-sanity
  summarize_f "Functional Tests Ubuntu 20.04" Logs-20.04-functional
  summarize_f "Functional Tests Ubuntu 22.04" Logs-22.04-functional
  cat Summary.md >> $GITHUB_STEP_SUMMARY
  mkdir -p Summary
  mv *.md Summary
else
  test -f Summary/err-$1.md && cat Summary/err-$1.md >> $GITHUB_STEP_SUMMARY
fi
exit 0
