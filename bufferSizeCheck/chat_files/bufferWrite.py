# Writes the .chat files for the ISS data to check the best buffer size
# =============================================================================================== #
# Patrick MacGregor
# Nuclear Physics Research Group
# School of Physics and Astronomy
# The University of Manchester
# LAST EDITED: 24/10/18
# =============================================================================================== #
# Define the buffer sizes and the 
bigbufsize = [450, 5400, 10350, 15300, 20250, 25200, 30150, 35100, 40050, 45000]
wosize = [100,100,100,100,100,100,50,50,50,50]

# Write to a file
for i in range(0,len(bigbufsize)):
	outFileName = "GEBMerge-" + str(bigbufsize[i]) + ".chat"
	
	outFile = open(outFileName,"w")
	
	outFile.write('''
#
# chatfile for GEBMerge
#

echo

#---------------------------------------
# max coincidence events to create, listing etc

maxNoEvents 2000000000
TSlistelen 50  1 20
#           |  | +-- last bigbuf
#           |  +-- first bigbuf
#           +-- max TS to write per file

nprint 1

#---------------------------------------
# output chunk size

chunksiz 2000000000

#---------------------------------------
# bigbuf size and wosize
#    +--- big size makes program slow;
#         but it may be necessary sometimes
''')
	
	outFile.write("bigbufsize " + str(bigbufsize[i]) + "\n")
	outFile.write("wosize     " + str(wosize[i]) + "\n")
	outFile.write('''
#  |
#  +--- in percent of bigbufsize
#       written out per buffer

#---------------------------------------
# how often should we write 
# statistics to output

reportinterval 1000

#---------------------------------------
# TS jump recovery trigger limits
# (if timestamps jumps outside these
#  limits GTMerge will read forward to 
#  try and recover, 0.1 to 1 sec seems to works)
#  100000000 ==  1 sec
#  100000    ==  1 ms
#  100       ==  1 us
#  1         == 10 ns

dts_min  -10000000000
dts_max   10000000000

dtsfabort 50
dtsbabort 50
#         |
#         +----- abort recovery and accept TS
#                after this many tries in a row

#---------------------------------------
# enable to zzip output data file
# does not work at the moment

;zzipout
''')
	outFile.close()
	

