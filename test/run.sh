#!/bin/bash

cmpout() {
	local base=$1
	shift 1

	"$@" < $base.in | diff -u $base.out -
	S=(${PIPESTATUS[@]})
	[ ${S[0]} = 0 ] && [ ${S[1]} = 0 ]
}

doincheck() {
	local base=$1
	local check=$2
	shift 2

	local ret=0
	for i in $base/*.in; do
		$check ${i%%.in} "$@" || ret=1
	done

	return $ret
}

doincmp() {
	local base=$1
	shift

	doincheck $base cmpout "$@"
}

THIS=$1

. $THIS/run-test.sh
