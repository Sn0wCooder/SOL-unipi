#!/bin/bash
# richiesto un parametro: nome della cartella in cui salvare gli output del client

if [ $# -lt 1 ];then
    echo 'Errore: parametro richiesto'
    exit
fi

saveDir=$1

sockName="sock.sk"

commonConf="-f $sockName -t 200 -p"
#configurazioni di clients
confClient1="$commonConf -W client.c,server.c,server"
confClient2="$commonConf -c client.c"
confClient3="$commonConf -w ."

#setup array di configurazioni client
clientConf[0]=$confClient1
clientConf[1]=$confClient2
clientConf[2]=$confClient3

if test -f "$sockName"; then
    rm $sockName
fi
#creazione shell in bg con valgrind per ottenerne il PID tramite $!
#valgrind --leak-check=full --show-leak-kinds=all -v ./server configs/config2.txt &
./server configs/config2.txt &
serverPID=$!

#se la cartella già esiste, la cancello e la ricreerò tramite lo script
rm -rf $saveDir

if mkdir $saveDir;then
  for((i=0;$i < ${#clientConf[@]};i++));do
    nomeFile=$saveDir/outputClient$i.txt
    if touch $nomeFile;then
      ./client ${clientConf[$i]} &> $nomeFile & #redirigo output client sul file
    else
      echo 'Errore creazione file'
    fi
    sleep 1 #intervallo di creazione dei clients
  done
  kill -s SIGHUP $serverPID #invio SIGHUP al server
  wait $serverPID
else
  echo 'Errore creazione directory'
fi
