#!/bin/bash

THIS=$1

map() {
	local ret=0
	for i in $THIS/*.in; do
		$1 $2 ${i%%.in} || ret=1
	done
	return $ret
}


run() {
	./main < $2.in | $1 $2
	S=(${PIPESTATUS[@]})
	[ ${S[0]} = 0 ] && [ ${S[1]} = 0 ]
}

timedrun() {
	timeout 10 ./main < $2.in | $1 $2
	S=(${PIPESTATUS[@]})
	[ ${S[0]} = 124 ] && [ ${S[1]} = 0 ]
}

gold() {
	diff -u $1.out -
}

fn() {
	$1.fn
}

. $THIS/run-test.sh
