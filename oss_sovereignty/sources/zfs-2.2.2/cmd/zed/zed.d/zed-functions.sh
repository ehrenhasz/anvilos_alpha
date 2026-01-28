: "${ZED_LOCKDIR:="/var/lock"}"
: "${ZED_NOTIFY_INTERVAL_SECS:=3600}"
: "${ZED_NOTIFY_VERBOSE:=0}"
: "${ZED_RUNDIR:="/var/run"}"
: "${ZED_SYSLOG_PRIORITY:="daemon.notice"}"
: "${ZED_SYSLOG_TAG:="zed"}"
ZED_FLOCK_FD=8
zed_check_cmd()
{
    local cmd
    local rv=0
    for cmd; do
        if ! command -v "${cmd}" >/dev/null 2>&1; then
            zed_log_err "\"${cmd}\" not installed"
            rv=$((rv + 1))
        fi
    done
    return "${rv}"
}
zed_log_msg()
{
    logger -p "${ZED_SYSLOG_PRIORITY}" -t "${ZED_SYSLOG_TAG}" -- "$@"
}
zed_log_err()
{
    zed_log_msg "error: ${0
}
zed_lock()
{
    local lockfile="$1"
    local fd="${2:-${ZED_FLOCK_FD}}"
    local umask_bak
    local err
    [ -n "${lockfile}" ] || return
    if ! expr "${lockfile}" : '.*/' >/dev/null 2>&1; then
        lockfile="${ZED_LOCKDIR}/${lockfile}"
    fi
    umask_bak="$(umask)"
    umask 077
    eval "exec ${fd}>> '${lockfile}'"
    if ! err="$(flock --exclusive "${fd}" 2>&1)"; then
        zed_log_err "failed to lock \"${lockfile}\": ${err}"
    fi
    umask "${umask_bak}"
}
zed_unlock()
{
    local lockfile="$1"
    local fd="${2:-${ZED_FLOCK_FD}}"
    local err
    [ -n "${lockfile}" ] || return
    if ! expr "${lockfile}" : '.*/' >/dev/null 2>&1; then
        lockfile="${ZED_LOCKDIR}/${lockfile}"
    fi
    if ! err="$(flock --unlock "${fd}" 2>&1)"; then
        zed_log_err "failed to unlock \"${lockfile}\": ${err}"
    fi
    eval "exec ${fd}>&-"
}
zed_notify()
{
    local subject="$1"
    local pathname="$2"
    local num_success=0
    local num_failure=0
    zed_notify_email "${subject}" "${pathname}"; rv=$?
    [ "${rv}" -eq 0 ] && num_success=$((num_success + 1))
    [ "${rv}" -eq 1 ] && num_failure=$((num_failure + 1))
    zed_notify_pushbullet "${subject}" "${pathname}"; rv=$?
    [ "${rv}" -eq 0 ] && num_success=$((num_success + 1))
    [ "${rv}" -eq 1 ] && num_failure=$((num_failure + 1))
    zed_notify_slack_webhook "${subject}" "${pathname}"; rv=$?
    [ "${rv}" -eq 0 ] && num_success=$((num_success + 1))
    [ "${rv}" -eq 1 ] && num_failure=$((num_failure + 1))
    zed_notify_pushover "${subject}" "${pathname}"; rv=$?
    [ "${rv}" -eq 0 ] && num_success=$((num_success + 1))
    [ "${rv}" -eq 1 ] && num_failure=$((num_failure + 1))
    [ "${num_success}" -gt 0 ] && return 0
    [ "${num_failure}" -gt 0 ] && return 1
    return 2
}
zed_notify_email()
{
    local subject="${1:-"ZED notification"}"
    local pathname="${2:-"/dev/null"}"
    : "${ZED_EMAIL_PROG:="mail"}"
    : "${ZED_EMAIL_OPTS:="-s '@SUBJECT@' @ADDRESS@"}"
    if [ -n "${ZED_EMAIL}" ] && [ -z "${ZED_EMAIL_ADDR}" ]; then
        ZED_EMAIL_ADDR="${ZED_EMAIL}"
    fi
    [ -n "${ZED_EMAIL_ADDR}" ] || return 2
    zed_check_cmd "${ZED_EMAIL_PROG}" || return 1
    [ -n "${subject}" ] || return 1
    if [ ! -r "${pathname}" ]; then
        zed_log_err \
                "${ZED_EMAIL_PROG
        return 1
    fi
    ZED_EMAIL_OPTS_PARSED="$(echo "${ZED_EMAIL_OPTS}" \
        | sed   -e "s/@ADDRESS@/${ZED_EMAIL_ADDR}/g" \
                -e "s/@SUBJECT@/${subject}/g")"
    {
        if [ "${ZED_EMAIL_OPTS%@SUBJECT@*}" = "${ZED_EMAIL_OPTS}" ] ; then
            printf "Subject: %s\n" "${subject}"
        fi
        cat "${pathname}"
    } |
    eval ${ZED_EMAIL_PROG} ${ZED_EMAIL_OPTS_PARSED} >/dev/null 2>&1
    rv=$?
    if [ "${rv}" -ne 0 ]; then
        zed_log_err "${ZED_EMAIL_PROG
        return 1
    fi
    return 0
}
zed_notify_pushbullet()
{
    local subject="$1"
    local pathname="${2:-"/dev/null"}"
    local msg_body
    local msg_tag
    local msg_json
    local msg_out
    local msg_err
    local url="https://api.pushbullet.com/v2/pushes"
    [ -n "${ZED_PUSHBULLET_ACCESS_TOKEN}" ] || return 2
    [ -n "${subject}" ] || return 1
    if [ ! -r "${pathname}" ]; then
        zed_log_err "pushbullet cannot read \"${pathname}\""
        return 1
    fi
    zed_check_cmd "awk" "curl" "sed" || return 1
    msg_body="$(awk '{ ORS="\\n" } { gsub(/\\/, "\\\\"); gsub(/"/, "\\\"");
        gsub(/\t/, "\\t"); gsub(/\f/, "\\f"); gsub(/\r/, "\\r"); print }' \
        "${pathname}")"
    [ -n "${ZED_PUSHBULLET_CHANNEL_TAG}" ] && msg_tag="$(printf \
        '"channel_tag": "%s", ' "${ZED_PUSHBULLET_CHANNEL_TAG}")"
    msg_json="$(printf '{%s"type": "note", "title": "%s", "body": "%s"}' \
        "${msg_tag}" "${subject}" "${msg_body}")"
    msg_out="$(curl -u "${ZED_PUSHBULLET_ACCESS_TOKEN}:" -X POST "${url}" \
        --header "Content-Type: application/json" --data-binary "${msg_json}" \
        2>/dev/null)"; rv=$?
    if [ "${rv}" -ne 0 ]; then
        zed_log_err "curl exit=${rv}"
        return 1
    fi
    msg_err="$(echo "${msg_out}" \
        | sed -n -e 's/.*"error" *:.*"message" *: *"\([^"]*\)".*/\1/p')"
    if [ -n "${msg_err}" ]; then
        zed_log_err "pushbullet \"${msg_err}"\"
        return 1
    fi
    return 0
}
zed_notify_slack_webhook()
{
    [ -n "${ZED_SLACK_WEBHOOK_URL}" ] || return 2
    local subject="$1"
    local pathname="${2:-"/dev/null"}"
    local msg_body
    local msg_tag
    local msg_json
    local msg_out
    local msg_err
    local url="${ZED_SLACK_WEBHOOK_URL}"
    [ -n "${subject}" ] || return 1
    if [ ! -r "${pathname}" ]; then
        zed_log_err "slack webhook cannot read \"${pathname}\""
        return 1
    fi
    zed_check_cmd "awk" "curl" "sed" || return 1
    msg_body="$(awk '{ ORS="\\n" } { gsub(/\\/, "\\\\"); gsub(/"/, "\\\"");
        gsub(/\t/, "\\t"); gsub(/\f/, "\\f"); gsub(/\r/, "\\r"); print }' \
        "${pathname}")"
    msg_json="$(printf '{"text": "*%s*\\n%s"}' "${subject}" "${msg_body}" )"
    msg_out="$(curl -X POST "${url}" \
        --header "Content-Type: application/json" --data-binary "${msg_json}" \
        2>/dev/null)"; rv=$?
    if [ "${rv}" -ne 0 ]; then
        zed_log_err "curl exit=${rv}"
        return 1
    fi
    msg_err="$(echo "${msg_out}" \
        | sed -n -e 's/.*"error" *:.*"message" *: *"\([^"]*\)".*/\1/p')"
    if [ -n "${msg_err}" ]; then
        zed_log_err "slack webhook \"${msg_err}"\"
        return 1
    fi
    return 0
}
zed_notify_pushover()
{
    local subject="$1"
    local pathname="${2:-"/dev/null"}"
    local msg_body
    local msg_out
    local msg_err
    local url="https://api.pushover.net/1/messages.json"
    [ -n "${ZED_PUSHOVER_TOKEN}" ] && [ -n "${ZED_PUSHOVER_USER}" ] || return 2
    if [ ! -r "${pathname}" ]; then
        zed_log_err "pushover cannot read \"${pathname}\""
        return 1
    fi
    zed_check_cmd "curl" "sed" || return 1
    msg_body="$(cat "${pathname}")"
    if [ -z "${msg_body}" ]
    then
        msg_body=$subject
        subject=""
    fi
    msg_out="$( \
        curl \
        --form-string "token=${ZED_PUSHOVER_TOKEN}" \
        --form-string "user=${ZED_PUSHOVER_USER}" \
        --form-string "message=${msg_body}" \
        --form-string "title=${subject}" \
        "${url}" \
        2>/dev/null \
        )"; rv=$?
    if [ "${rv}" -ne 0 ]; then
        zed_log_err "curl exit=${rv}"
        return 1
    fi
    msg_err="$(echo "${msg_out}" \
        | sed -n -e 's/.*"errors" *:.*\[\(.*\)\].*/\1/p')"
    if [ -n "${msg_err}" ]; then
        zed_log_err "pushover \"${msg_err}"\"
        return 1
    fi
    return 0
}
zed_rate_limit()
{
    local tag="$1"
    local interval="${2:-${ZED_NOTIFY_INTERVAL_SECS}}"
    local lockfile="zed.zedlet.state.lock"
    local lockfile_fd=9
    local statefile="${ZED_RUNDIR}/zed.zedlet.state"
    local time_now
    local time_prev
    local umask_bak
    local rv=0
    [ -n "${tag}" ] || return 0
    zed_lock "${lockfile}" "${lockfile_fd}"
    time_now="$(date +%s)"
    time_prev="$(grep -E "^[0-9]+;${tag}\$" "${statefile}" 2>/dev/null \
        | tail -1 | cut -d\; -f1)"
    if [ -n "${time_prev}" ] \
            && [ "$((time_now - time_prev))" -lt "${interval}" ]; then
        rv=1
    else
        umask_bak="$(umask)"
        umask 077
        grep -E -v "^[0-9]+;${tag}\$" "${statefile}" 2>/dev/null \
            > "${statefile}.$$"
        echo "${time_now};${tag}" >> "${statefile}.$$"
        mv -f "${statefile}.$$" "${statefile}"
        umask "${umask_bak}"
    fi
    zed_unlock "${lockfile}" "${lockfile_fd}"
    return "${rv}"
}
zed_guid_to_pool()
{
	if [ -z "$1" ] ; then
		return
	fi
	guid="$(printf "%u" "$1")"
	$ZPOOL get -H -ovalue,name guid | awk '$1 == '"$guid"' {print $2; exit}'
}
zed_exit_if_ignoring_this_event()
{
	if [ -n "${ZED_SYSLOG_SUBCLASS_INCLUDE}" ]; then
	    eval "case ${ZEVENT_SUBCLASS} in
	    ${ZED_SYSLOG_SUBCLASS_INCLUDE});;
	    *) exit 0;;
	    esac"
	elif [ -n "${ZED_SYSLOG_SUBCLASS_EXCLUDE}" ]; then
	    eval "case ${ZEVENT_SUBCLASS} in
	    ${ZED_SYSLOG_SUBCLASS_EXCLUDE}) exit 0;;
	    *);;
	    esac"
	fi
}
