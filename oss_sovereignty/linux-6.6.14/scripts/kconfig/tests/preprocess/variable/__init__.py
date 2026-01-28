"""
Variable and user-defined function tests.
"""
def test(conf):
    assert conf.oldaskconfig() == 0
    assert conf.stderr_matches('expected_stderr')
