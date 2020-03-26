#!/usr/bin/awk

/^\(eclair\) info threads/ { st = 1; next; }
/^\(eclair\) thread 3/ { st = 3; next; }
/^\(eclair\) bt/ { st = 4; next; }
/^\(eclair\) p\/x _end/ { st = 5; next; }

st == 1 { st = 2; next; }

st == 2 {
    if ($0 ~ /Thread [0-9]* .* at .*:[0-9]*/) {
	printf "[ OK ] "
	ok++
    } else {
print $0
	printf "[ NG ] "
	ng++
    }
    print "info threads"
    st = 0
}

st == 3 {
    if ($0 ~ /Switching to thread 3/) {
	printf "[ OK ] "
	ok++
    } else {
	printf "[ NG ] "
	ng++
    }
    print "switch thread"
    st = 0
}

st == 4 {
    if ($0 ~ /at .*:[0-9]*/) {
	printf "[ OK ] "
	ok++
    } else {
	printf "[ NG ] "
	ng++
    }
    print "backtrace"
    st = 0
}


st == 5 {
    if ($3 ~ /0x[0-9a-f]{16}/) {
	printf "[ OK ] "
	ok++
    } else {
	printf "[ NG ] "
	ng++
    }
    print "_end address shown"
    st = 0
}

END {
    exit (ok + ng != 4 || ng > 0) ? 1 : 0
}
