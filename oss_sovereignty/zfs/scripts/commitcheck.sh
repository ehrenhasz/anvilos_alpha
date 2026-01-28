REF="HEAD"
test_commit_bodylength()
{
    length="72"
    body=$(git log --no-show-signature -n 1 --pretty=%b "$REF" | grep -Ev "http(s)*://" | grep -E -m 1 ".{$((length + 1))}")
    if [ -n "$body" ]; then
        echo "error: commit message body contains line over ${length} characters"
        return 1
    fi
    return 0
}
check_tagged_line()
{
    regex='^[[:space:]]*'"$1"':[[:space:]][[:print:]]+[[:space:]]<[[:graph:]]+>$'
    foundline=$(git log --no-show-signature -n 1 "$REF" | grep -E -m 1 "$regex")
    if [ -z "$foundline" ]; then
        echo "error: missing \"$1\""
        return 1
    fi
    return 0
}
new_change_commit()
{
    error=0
    long_subject=$(git log --no-show-signature -n 1 --pretty=%s "$REF" | grep -E -m 1 '.{73}')
    if [ -n "$long_subject" ]; then
        echo "error: commit subject over 72 characters"
        error=1
    fi
    if ! check_tagged_line "Signed-off-by" ; then
        error=1
    fi
    if ! test_commit_bodylength ; then
        error=1
    fi
    return "$error"
}
is_coverity_fix()
{
    subject=$(git log --no-show-signature -n 1 --pretty=%s "$REF" | grep -E -m 1 '^Fix coverity defects')
    if [ -n "$subject" ]; then
        return 0
    fi
    return 1
}
coverity_fix_commit()
{
    error=0
    subject=$(git log --no-show-signature -n 1 --pretty=%s "$REF" |
        grep -E -m 1 'Fix coverity defects: CID [[:digit:]]+(, [[:digit:]]+)*')
    if [ -z "$subject" ]; then
        echo "error: Coverity defect fixes must have a subject line that starts with \"Fix coverity defects: CID dddd\""
        error=1
    fi
    if ! check_tagged_line "Signed-off-by" ; then
        error=1
    fi
    OLDIFS=$IFS
    IFS='
'
    for line in $(git log --no-show-signature -n 1 --pretty=%b "$REF" | grep -E '^CID'); do
        if ! echo "$line" | grep -qE '^CID [[:digit:]]+: ([[:graph:]]+|[[:space:]])+ \(([[:upper:]]|\_)+\)'; then
            echo "error: commit message has an improperly formatted CID defect line"
            error=1
        fi
    done
    IFS=$OLDIFS
    if ! test_commit_bodylength; then
        error=1
    fi
    return "$error"
}
if [ -n "$1" ]; then
    REF="$1"
fi
if is_coverity_fix; then
    if ! coverity_fix_commit; then
        exit 1
    else
        exit 0
    fi
fi
if ! new_change_commit ; then
    exit 1
fi
exit 0
