#!/bin/bash
if [ $# == 6 ]; then
    while getopts s:i:d: option; do
        case "${option}" in
        s) SERVER=${OPTARG} ;;
        i) ITERATIONS=${OPTARG} ;;
        d) TMPDIR=${OPTARG} ;;
        esac
    done

else
    echo "Error: Insufficient number of arguments"
    echo "Usage: http_test.sh -s 192.168.3.222 -i 10 -d tmp"
    exit 1
fi

# SERVER=192.168.3.222
# ITERATIONS=10
# TMPDIR=tmp

DISTDIR=storage
echo "Running test for server=$SERVER, iterations=$ITERATIONS, tempdir=$TMPDIR"

RESOURCE=([0]="index.html" [1]="index.css" [2]="index.js")
FILENAME=([0]="index.html.gz" [1]="index.css.gz" [2]="index.js.gz")
RESTYPE=([0]="binary" [1]="binary" [2]="binary")

RESCOUNT=3

FAILTOLOAD=0
BADMD5HASH=0
OK=0
TOTAL=0

mkdir -p $TMPDIR
rm -f $TMPDIR/*

START=$(date +%s)

for ((J = 0; J < $ITERATIONS; J++)); do
    for I in ${!RESOURCE[*]}; do
        echo -e "Requesting ${RESOURCE[$I]}:"
        TOTAL=$((TOTAL + 1))
        tmp="$(curl http://$SERVER/${RESOURCE[$I]} -sD - -w '\nTIMEELAPSED: %{time_total}\n' --connect-timeout 10 --max-time 10 --header 'Accept-Encoding: gzip, deflate' -o ./${TMPDIR}/${J}-${FILENAME[$I]} | grep "MD5HASH\|TIMEELAPSED")"
        ret=$?
        md5header=$(echo "${tmp}" | grep "MD5HASH" | awk '{print $2}')
        loadtime=$(echo "${tmp}" | grep "TIMEELAPSED" | awk '{print $2}')
        echo "RESOURCE: ${J}-${FILENAME[$I]} LOAD TIME: $loadtime s Header MD5 HASH: $md5header"
        if [[ $ret -ne 0 ]]; then
            FAILTOLOAD=$((FAILTOLOAD + 1))
            echo -e "\x1b[31m${FILENAME[$I]} FAIL TO LOAD\x1b[0m"
        else
            if [ ${#md5header} -gt 0 ]; then
                md5header="${md5header:0:-1}"
            fi
            md5file=$(md5sum ./$TMPDIR/${J}-${FILENAME[$I]} | awk '{print $1}')
            echo "File ./${TMPDIR}/${J}-${FILENAME[$I]} MD5 HASH: $md5file"
            if [[ "$md5file" == "$md5header" ]]; then
                OK=$((OK + 1))
                echo -e "\x1b[32mMD5 HASH OK\x1b[0m"
            else
                BADMD5HASH=$((BADMD5HASH + 1))
                echo -e "\x1b[31m${J}-${FILENAME[$I]} FAIL MD5 HASH\x1b[0m"
            fi
        fi
        rm -f ./$TMPDIR/${J}-${FILENAME[$I]}
    done
done

END=$(date +%s)
DIFF=$(($END - $START))

echo -e "\n"
echo -e "TEST $SERVER IN '$TMPDIR' DIRECTORY TOOK: $DIFF seconds"
echo -e "TOTAL REQUESTS: $TOTAL - 100%"
echo -e "\x1b[32mSUCCESS: $OK - $(((OK * 100) / TOTAL))%\x1b[0m"
echo -e "\x1b[31mFAIL TO LOAD: $FAILTOLOAD - $(((FAILTOLOAD * 100) / TOTAL))%\x1b[0m"
echo -e "\x1b[31mFAIL MD5 HASH: $BADMD5HASH - $(((BADMD5HASH * 100) / TOTAL))%\x1b[0m"

#rm -R $TMPDIR
