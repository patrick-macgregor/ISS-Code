#!/bin/sh

if [ $# -ne 1 ] 
  then
   echo "Specify only the run number to sort"
  exit 1
fi

RUN=$1

exp=iss631
dir=/Users/heliosdigios/experiments/${exp}

echo "RUN $RUN: GEBMerge started at `date`"
 
$dir/analysis/GEBSort/GEBMerge $dir/analysis/working/GEBMerge.chat  $dir/merged_data/GEBMerged_run$RUN.gtd `ls $dir/data/${exp}_run_$RUN.gtd*`
 
echo "RUN $RUN: GEBMerge DONE at `date`"
