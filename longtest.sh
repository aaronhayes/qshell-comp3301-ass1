#!/bin/sh

# The following is a here document.
# It takes everything between the ___EOF___ as input to qshell.
# This should run just as if you typed each command in at the prompt.
./qshell <<___EOF___
echo No Arguments Command Test
echo ps
ps
echo Command with Arguments Test
echo ps -l
ps -l
echo Output Redirection Test
echo  ps -l  outputredirect myFile
ps -l > myFile
echo Input Redirection Test
echo wc inputredirect myFile
wc < myFile
echo Comment Test
# Does qshell treat this as a comment?
echo Blank Line Test

# special < characters | Ok in > comments ?
#Test Pipes
echo ls PIPE wc
ls | wc
# Pipes plus redirection
echo sort FROM myFile PIPE wc TO myFile2
sort < myFile | wc  > myFile2
cat myFile2
# background
echo ls -R /usr TO test 2 BG
ls -R /usr > test2 &
echo sleep 10
sleep 10
echo wc test2
wc test2
echo auto input file redirection for background tasks
echo wc BG
wc &
#cd command
echo mkdir testdir
mkdir testdir
echo ls -l TO testdir/testdirfile
ls -l > testdir/testdirfile
echo cd testdir
cd testdir
echo ls -l 
ls -l
echo cd ..
cd ..
#some Error conditions
echo more than 20 arguments
echo this is a really long line that might have more args than needs to be dealt with by the shell in one line and might fail
echo this is a really long line that might have more args than needs to be dealt with by the shell in one line and might fail but should do so correctly > myFile3
cat myFile3
echo another line longer than 128 chars
echo this is a really long line that has more than 128 chars which is more than needs to be dealt with by the shell in one line and might fail but should do so correctly > myFile4
cat myFile4
#echo this is a really long line that has | more than 128 chars which is more than needs to be dealt with by the shell in one line and might fail but should do so correctly > myFile4


exit
___EOF___
# here document done


