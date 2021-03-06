#!/bin/sh

if [ $# -eq 0 ]
then
    read -p 'Please enter the run number you would like to process: ' RUN
    read -p 'Please enter the system you are running on (1-MAC, 2-ONENET, 3-LOCAL): ' WHERE
elif [ $# -eq 1 ]
then
    RUN=$1
    WHERE=3
    
else
    RUN=$1
    WHERE=$2
fi


exp=iss631
expDir=/home/ptmac/Documents/07-CERN-ISS-Mg/analysis
ATTEMPTS=0

while [ "${ATTEMPTS}" -lt "10000" ];
do
    
#    rsync -rtuh --delete --progress rsync://helios@192.168.1.2:12000/digiosdata /Users/heliosdigios/experiments/infl001_19O/data/.
    #not needed when sync daemon running
    #/Users/heliosdigios/Applications/get_digios_data.sh $RUN $WHERE
    #/Users/heliosdigios/experiments/infl001_19O/working/gebmerge.sh $RUN
    #/Users/heliosdigios/experiments/infl001_19O/working/gebsortmerged.sh $RUN
    ${expDir}/working/gebmerge_local.sh $RUN
    ${expDir}/working/gebsortmerged_local.sh $RUN
    
    echo Just created root file run${RUN}.root in ${expDir}/root_data
    ls -ltrh ${expDir}/root_data
    
    root -q -b "process_run_local.C(${RUN},0)"
    cp gen.root ../root_data/gen_run${RUN}.root
    echo ** Copied gen.root to gen_run${RUN}.root
    
    echo ----Done with Processing Run Number ${RUN}----
    echo "Finished Processing Attempt Number = ${ATTEMPTS}"
    echo "Will start again in 10 seconds....."
    
    ATTEMPTS=$(expr ${ATTEMPTS} + 1)
    echo '*' 
    sleep 2
    echo '**' 
    sleep 2
    echo '***' 
    sleep 2
    echo '****' 
    sleep 2
    echo '*****' 
    sleep 2
    
done

