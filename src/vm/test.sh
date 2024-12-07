cnt=0

for (( i=0; i<20; i=i+1 ))
  do
    make clean && make
    cd build
    echo "$cnt / 20"
    make check > test.txt

    output=$( tail -n 1 test.txt )
    output=($output)

    IFS=' ' read -r -a array <<< "${output[0]}"
    failed_cnt=${array[0]}

    if [ $failed_cnt == "All" ]
    then
        msg="\n\nPASS\n\n"
        echo -e $msg
        cnt=$((cnt+1))
        cd ../
    else
        echo "Fail!!!"
        cd ../
        tar -cvf ../../build_$cnt.tar build
        break
    fi
    
done

rm test.txt

if [ $cnt -eq 20 ]
then
    echo "Successfully passed all tests!"
fi