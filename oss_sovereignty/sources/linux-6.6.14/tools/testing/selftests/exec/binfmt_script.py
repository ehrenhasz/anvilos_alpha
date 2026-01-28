import os, subprocess
SIZE=256
NAME_MAX=int(subprocess.check_output(["getconf", "NAME_MAX", "."]))
test_num=0
code='''
print "Executed interpreter! Args:\n";
print "0 : '$0'\n";
$counter = 1;
foreach my $a (@ARGV) {
    print "$counter : '$a'\n";
    $counter++;
}
'''
def test(name, size, good=True, leading="", root="./", target="/perl",
                     fill="A", arg="", newline="\n", hashbang="
    global test_num, tests, NAME_MAX
    test_num += 1
    if test_num > tests:
        raise ValueError("more binfmt_script tests than expected! (want %d, expected %d)"
                         % (test_num, tests))
    middle = ""
    remaining = size - len(hashbang) - len(leading) - len(root) - len(target) - len(arg)
    while remaining >= NAME_MAX:
        middle += fill * (NAME_MAX - 1)
        middle += '/'
        remaining -= NAME_MAX
    middle += fill * remaining
    dirpath = root + middle
    binary = dirpath + target
    if len(target):
        os.makedirs(dirpath, mode=0o755, exist_ok=True)
        open(binary, "w").write(code)
        os.chmod(binary, 0o755)
    buf=hashbang + leading + root + middle + target + arg + newline
    if len(newline) > 0:
        buf += 'echo this is not really perl\n'
    script = "binfmt_script-%s" % (name)
    open(script, "w").write(buf)
    os.chmod(script, 0o755)
    proc = subprocess.Popen(["./%s" % (script)], shell=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    stdout = proc.communicate()[0]
    if proc.returncode == 0 and b'Executed interpreter' in stdout:
        if good:
            print("ok %d - binfmt_script %s (successful good exec)"
                  % (test_num, name))
        else:
            print("not ok %d - binfmt_script %s succeeded when it should have failed"
                  % (test_num, name))
    else:
        if good:
            print("not ok %d - binfmt_script %s failed when it should have succeeded (rc:%d)"
                  % (test_num, name, proc.returncode))
        else:
            print("ok %d - binfmt_script %s (correctly failed bad exec)"
                  % (test_num, name))
    os.unlink(script)
    if len(target):
        elements = binary.split('/')
        os.unlink(binary)
        elements.pop()
        while len(elements) > 1:
            os.rmdir("/".join(elements))
            elements.pop()
tests=27
print("TAP version 1.3")
print("1..%d" % (tests))
test(name="too-big",        size=SIZE+80, good=False)
test(name="exact",          size=SIZE,    good=False)
test(name="exact-space",    size=SIZE,    good=False, leading=" ")
test(name="whitespace-too-big", size=SIZE+71, good=False, root="",
                                              fill=" ", target="")
test(name="truncated",      size=SIZE+17, good=False, leading=" " * 19)
test(name="empty",          size=2,       good=False, root="",
                                          fill="", target="", newline="")
test(name="spaces",         size=SIZE-1,  good=False, root="", fill=" ",
                                          target="", newline="")
test(name="newline-prefix", size=SIZE-1,  good=False, leading="\n",
                                          root="", fill=" ", target="")
test(name="test.pl",        size=439, leading=" ",
     root="./nix/store/bwav8kz8b3y471wjsybgzw84mrh4js9-perl-5.28.1/bin",
     arg=" -I/nix/store/x6yyav38jgr924nkna62q3pkp0dgmzlx-perl5.28.1-File-Slurp-9999.25/lib/perl5/site_perl -I/nix/store/ha8v67sl8dac92r9z07vzr4gv1y9nwqz-perl5.28.1-Net-DBus-1.1.0/lib/perl5/site_perl -I/nix/store/dcrkvnjmwh69ljsvpbdjjdnqgwx90a9d-perl5.28.1-XML-Parser-2.44/lib/perl5/site_perl -I/nix/store/rmji88k2zz7h4zg97385bygcydrf2q8h-perl5.28.1-XML-Twig-3.52/lib/perl5/site_perl")
test(name="one-under",           size=SIZE-1)
test(name="two-under",           size=SIZE-2)
test(name="exact-trunc-whitespace", size=SIZE, arg=" ")
test(name="exact-trunc-arg",     size=SIZE, arg=" f")
test(name="one-under-full-arg",  size=SIZE-1, arg=" f")
test(name="one-under-no-nl",     size=SIZE-1, newline="")
test(name="half-under-no-nl",    size=int(SIZE/2), newline="")
test(name="one-under-trunc-arg", size=SIZE-1, arg=" ")
test(name="one-under-leading",   size=SIZE-1, leading=" ")
test(name="one-under-leading-trunc-arg",  size=SIZE-1, leading=" ", arg=" ")
test(name="two-under-no-nl",     size=SIZE-2, newline="")
test(name="two-under-trunc-arg", size=SIZE-2, arg=" ")
test(name="two-under-leading",   size=SIZE-2, leading=" ")
test(name="two-under-leading-trunc-arg",   size=SIZE-2, leading=" ", arg=" ")
test(name="two-under-no-nl",     size=int(SIZE/2), newline="")
test(name="two-under-trunc-arg", size=int(SIZE/2), arg=" ")
test(name="two-under-leading",   size=int(SIZE/2), leading=" ")
test(name="two-under-lead-trunc-arg", size=int(SIZE/2), leading=" ", arg=" ")
if test_num != tests:
    raise ValueError("fewer binfmt_script tests than expected! (ran %d, expected %d"
                     % (test_num, tests))
