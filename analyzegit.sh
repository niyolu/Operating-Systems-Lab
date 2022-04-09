#!/bin/bash
#bash script that analyses a git checkout


#help-message
function helper(){
	echo "Usage: analysegit [DIRECTORY]... [OPTION]"
	echo "Analyze and display a Git-Checkout"
	echo "Options:"
	echo "-n, --number		count amount of commits (default)"
	echo "-a, --author 		list author and his amount of commits"
	echo "-m, --month			list amount of commits per month"
}


#set checkout path and manage as well as
#perform error handling on paramteres
if [ -d $1 ]; then
	DIRECTORY=$1
	shift
	if [ $# -gt 2 ]; then
		helper
		exit 1
	fi
else
	if [ $# -gt 1 ]; then
		helper
		exit 1
	fi
	DIRECTORY=$PWD
fi

cd $DIRECTORY

#get optional params
MODE=${1:-"-n"}

#choose functionality
case $MODE in
"-n"|"--number")

			#log commits as blank, then count lines

			echo "Total amount of commits: `git log --pretty=format:"" |wc -l`"
			;;

"-a"|"--author")

			#get number of commits per author

			git shortlog -s --all | cat
			;;

"-m"|"--month")

			#get number of commits per month

			CURRENT=`git log --pretty=format:"%ai" | tail -1 | cut -c -7`
			LAST=`git log --pretty=format:"%ai" | head -1 | cut -c -7`

			until [[ "$LAST" < "$CURRENT" ]]; do

				#count commits with current date
				N=`git log --pretty=format:"%ai" | cut -c -7 | grep -c "$CURRENT"`

				#use printf instead of echo for integer padding
				#6 decimals should suffice, contribution record is a 5-digit
				printf "[%s]:%6d\n" $CURRENT $N
				
				#increase, then format date
				CURRENT=$(date -I -d "$CURRENT - 1 month")
				CURRENT=$( echo "$CURRENT" | cut -c -7 )

			done
			;;
			
"-h"|"--help")
			helper
			;;

*)
			echo "Invalid argument"
			helper

esac
