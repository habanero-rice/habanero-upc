#!/usr/bin/env python

###########################################
#######  Author: VIVEK KUMAR (vivek.kumar@rice.edu) #######
###########################################

############## DESCRIPTION ################
#
# This python script is designed to read  
# C files in hpgmg folder and convert all
# OpenMP for-loops into HabaneroUPC++ 
# forasyncs. This script is not very 
# generalized to handle all possible declarations
# of for-loop. See the TODOs marked below
# explaining the limitations. However,
# this version of script works perfect 
# with hpgmg git commit:
# 66ee7ea
#
#
# USAGE: ./scriptName.py <Top level dir to parse> <Sequential codegen ? 0 : 1>
#
# Running this script as above will create 
# a directory "hcupcxx" in current working
# directory under which the original
# directory tree appears, containing 
# all the converted files.
#
# All for-loop with any OpenMP pragmas, are
# modified as shown below:
# ORIGINAL:::
#
# int itrID;
# for(itrID=lowBound; itrID<highBound; itrID++) {
#     // FOR LOOP ACTUAL BODY
# }
#  
# MODIFIED:::
#// -------------------->>>>
#  {
#  const int _factor = (int) (FORASYNC_LAMBDA_TILING_FACTOR * highBound);
#  const int _ltile = _factor == 0 ? 1 : _factor;
#  const int _chunks = (int) (highBound/_ltile);
#  const int _sizeB = _ltile * _chunks;
#  auto _lambda = [&](int _outter) {
#    const int _start = _outter * _ltile;
#    for(int itrID=_start; itrID<(_start+_ltile); itrID++) {
#      // FOR LOOP ACTUAL BODY
#    }
#  });
#
#  hupcpp::loop_domain_t loop_t = {lowBound, _chunks, 1, 1};
#  hupcpp::start_finish();
#  hupcpp::forasync1D<decltype(_lambda)>(&loop_t, lambda, FORASYNC_MODE_RECURSIVE);
#  hupcpp::end_finish();
#
#  if(_sizeB < highBound) {
#    for(itrID=_sizeB; itrID<highBound; itrID++) {
#      // FOR LOOP ACTUAL BODY
#    }
#  }
#  }
#// <<<<--------------------
##########################################

import re
import sys
import os
import shutil

# Global variable declarations
# When user passes 2nd command line arg as 1, then it means dry-run of script
DEBUG = 0
sequential = 0
HC_WORKERS = "24"
HC_WID = "hupcpp::get_hc_wid()"
testOnly = 0
outDir = os.getcwd() + "/hcupcxx"
logFile = os.getcwd() + "/parsing.log"
logFileFD = open(logFile, 'w')

###### Function declarations

def throw_err(fileName, lineNum, msg):
  if fileName == 0:
    print "Error: ", msg, ".. Exiting."
  else:
    print "Error(", fileName, ":", lineNum, "): ", msg, ".. Exiting."
  sys.exit(-1)

# replace multiple strings in line
def replace_all(text, dic):
  for i, j in dic.iteritems():
    text = text.replace(i, j)
  return text

# read a file by using line number
def give_line(line_number, inFileFD, dict_pos):
  inFileFD.seek(dict_pos.get(line_number))
  line = inFileFD.readline()
  return line

# write the signature of the inner function 
# for the corresponding for-loop
def lambda_signature(iter, lowBound, highBound, reduction_val_name, outFileFD):
  # If this a reduction pragma, then enable reduction using arrays
  outFileFD.write("{\n") # Start brace for parallel code
  if len(reduction_val_name) > 0:
    outFileFD.write("double _" + reduction_val_name + "_[" + HC_WORKERS + "];\n")
    outFileFD.write("memset(_" + reduction_val_name + "_, 0, sizeof(double)*" + HC_WORKERS + ");\n")
    outFileFD.write("\n")

  outFileFD.write("const int _factor = (int) (FORASYNC_LAMBDA_TILING_FACTOR * " + highBound + ");\n")
  outFileFD.write("const int _ltile = _factor == 0 ? 1 : _factor;\n")
  outFileFD.write("const int _chunks = (int) (" + highBound + "/_ltile);\n")
  #outFileFD.write("if(_chunks != " + highBound + ") ")
  #outFileFD.write("printf(\"CRT: File:%s, line:%d, chunks:%d, highBound:%d\\n\",__FILE__,__LINE__,_chunks, " + highBound + ");\n")
  outFileFD.write("const int _sizeB = _ltile * _chunks;\n")
  lambdaName = "_lambda"
  outFileFD.write("auto " + lambdaName + " = ")
  if len(reduction_val_name) > 0:
    outFileFD.write("[=, &_" + reduction_val_name + "_] (int _outter) {\n")
  else:
    outFileFD.write("[=] (int _outter) {\n")
  outFileFD.write("const int _start = _outter * _ltile;\n")
  outFileFD.write("for(int " + iter + "=_start; " + iter + "<(_start+_ltile); " + iter + "++) \n")
  outFileFD.write("\n")
  return lambdaName

# Call the inner function after its declaration
def lambda_call(lambdaName, iter, lowBound, highBound, forloopBody, outFileFD):
  # Close the lambda function
  outFileFD.write("}; \n")
  #outFileFD.write("printf(\"CRT: File:%s, line:%d\\n\",__FILE__,__LINE__);\n")
  #outFileFD.write("printf(\"CRT: File:%s, line:%d, HIGH:%d\\n\",__FILE__,__LINE__, " + highBound  + ");\n")

  outFileFD.write("hupcpp::loop_domain_t loop_t = {" + lowBound + ", _chunks, 1, 1};\n")
  outFileFD.write("hupcpp::start_finish();\n")
  outFileFD.write("hupcpp::forasync1D<decltype(_lambda)>(&loop_t, _lambda, FORASYNC_MODE_RECURSIVE);\n")
  outFileFD.write("hupcpp::end_finish();\n")
  
  outFileFD.write("if(_sizeB < " + highBound + ") {\n")
  outFileFD.write("for(int " + iter + "=_sizeB; " + iter + "<" + highBound + "; " + iter + "++) \n")
  outFileFD.write(forloopBody)
  outFileFD.write("}\n")

# Print the asserting failure for all #define PRAGMA_*
def print_assertMsg(line, outFileFD):
  assertStr = "\"Unexpected OpenMP pragma execution\""
  fields = re.split(r'MyPragma', line)
  if len(fields) == 0:
    # empty #define
    outFileFD.write(line + " ")
  else:  
    outFileFD.write(fields[0])
  outFileFD.write(" assert(" + assertStr + " && 0);")
  outFileFD.write("\n")

# Function to Ensure for-loop is declaring
# its boody using braces 
def ensure_forloopBody_within_braces(line, lineNum, inFileFD, dict_pos, inFile):
  # The open brace can either
  # be: a) on the same line as of for-loop; or b) on the
  # next line (TODO: Generalize)
        
  # check condition 'a' of above
  braces = line.count('{')
  if braces == 0:
    # check condition 'b' of above
    # brace on a new line which is only having blank spaces before '{'
    line = give_line(lineNum, inFileFD, dict_pos)
    lineNum = lineNum + 1
    braces = line.count('{')
    if braces == 0:
      # throw an error if an open brace not found
      throw_err(inFile, lineNum, "for-loop body not enclosed in braces")
  return braces, line, lineNum

# Parse a for loop deaclaration and find iteratorID, lower bound and higher bounds
def parse_forloop_properties(line, inFile, lineNum):
  # First parse the conditional arguments inside for-loop
  rep = {"for": "", "(": "", ")": "", "{": ""}
  line = replace_all(line, rep)
  # Create array
  fields = re.split(r';', line)

  # Find loop iterator and lowBound
  # TODO: Deal with multiple initializers
  itrID = fields[0]
  # split this on '=' (e.g. i=0)
  tmp = re.split(r'=', itrID)
  itrID = tmp[0]
  lowBound = tmp[1]
    
  # Find highBound
  # TODO: Currently we only assume limits given using '<' operator
  highBound = fields[1]
  condition = 0 # 0 indicating type '<'
  tmp = re.split(r'<', highBound)
  if highBound.count('=') > 0:
    condition = 1 # 1 indicating type '<='
    tmp = re.split(r'=', highBound)

  highBound = tmp[1]
  if condition == 1:
    # i<=10 ==> i<(10+1)
    highBound = "(" + highBound + "+1)"

  # Find stride
  # TODO: We only support '++'. Verify that here
  stride = 1
  if not re.match('(.*)\+\+(.*)', fields[2]):
    throw_err(inFile, lineNum, "Currently stride as one is only supported")

  return itrID, lowBound, highBound

# Extract the body of for-loop, without any modification -- as is.
def get_forloopBody(braces, line, lineNum, dict_pos, inFileFD):
  loopBody = ""
  if braces == 0:
    loopBody = line[line.index("{"):len(line)] + "\n"
  else:
    tmp = braces
    while tmp > 0:
      loopBody = loopBody + "{\n"
      tmp = tmp - 1
    while braces > 0:
      line = give_line(lineNum, inFileFD, dict_pos)
      lineNum = lineNum + 1
      oBrace = line.count('{')
      cBrace = line.count('}')
      braces = braces + oBrace - cBrace
      loopBody = loopBody + line + "\n"
  return loopBody, lineNum

# generate sequential for-loop -- used when highBound is very small
def seq_forloop(itr, lowBound, highBound, loopBody, ifoRelse, outFileFD):
  if ifoRelse == 0: #if type
    outFileFD.write("if( ")
  else:             # else if type
    outFileFD.write("else if(\n")

  leType = highBound.count('=')
  if leType > 0:
    rep = {"=": ""}
    highB = replace_all(highBound, rep)
    outFileFD.write(highB + "<= FORASYNC_SEQ_LIMIT) {\n")
  else:
    outFileFD.write(highBound + "<= FORASYNC_SEQ_LIMIT) {\n") 

  outFileFD.write("for(int " + itr + "=" + lowBound + "; " + itr + "<" + highBound + "; " + itr + "++)")
  outFileFD.write(loopBody)
  outFileFD.write("}\n")

def replaceReductionVariable(forloopBody, reduction_val_name, inFile):
  lines = re.split(r'\n', forloopBody)
  modified_forloopBody = ""

  for i in range(0, len(lines)):
    line = lines[i]
    if re.match('(.*)' + reduction_val_name + '(.*)', line):
      array_var = "_" + reduction_val_name + "_"
      # line for SUM will be as: "sum_level+=sum_block;"
      if re.match('(.*)\+=(.*)', line):
        fields = re.split(r'\+=', line);
        if not len(fields) == 2:
          throw_err(inFileFD, 0, "22Unable to parse reduction operation for reduction pragma")
        modified_forloopBody = modified_forloopBody + array_var + "[" + HC_WID + "] = " + array_var + "[" + HC_WID + "] + " + fields[1] + ";\n"
        continue
      # line for MAX will be as: "if(block_norm>max_norm){max_norm = block_norm;}"
      elif re.match('(.*)if(.*)=(.*)', line):
        rep = {reduction_val_name: array_var + "[" + HC_WID + "]"}
        line = replace_all(line, rep)
        modified_forloopBody = modified_forloopBody + line + "\n"
        continue
      else:
        throw_err(inFile, 0, "11Unable to parse reduction operation for reduction pragma")
    else:
      modified_forloopBody = modified_forloopBody + line + "\n"
  return modified_forloopBody

### Open input file and parse ###
def parse_file(fileName, cFile):
  outFile = outDir + "/" + fileName
  # find the dirname in this outFile
  outFileDIR = os.path.dirname(outFile)
  # create the directory (if any)
  if not os.path.exists(outFileDIR):
    os.makedirs(outFileDIR)
 
  if cFile == 0:
    # this is not a c-file and hence copy as-is
    shutil.copyfile(fileName,outFile)
    return
 
  inFile = fileName

  ### 3. Read file line by line ###
  lineNum = 0
  inFileFD = open(inFile, 'r')
  outFileFD = open(outFile, 'w')
  logging = 0
 
  # Complete dictionary with start position of each line (key is line number, 
  # and value - cumulated length of previous lines).
  length = 0
  dict_pos = {}
  for line in inFileFD:
    dict_pos[lineNum] = length
    length = length+len(line)
    lineNum = lineNum+1

  totalLines = lineNum
  lineNum = 0
  totalFors = 0

  while lineNum < totalLines:
    line = give_line(lineNum, inFileFD, dict_pos)
    lineNum = lineNum + 1

    # We consider a for loop as potential forasync only
    # if it has annotation PRAGMA_THREAD_ACROSS_BLOCKS(i.e., OpenMP pragma)
    line = line.rstrip()

    # We are replacing the OpenMP pragmas to forasync. Just to ensure
    # some of the pragmas are not left, we will redine OpenMP pragmas
    # in hpgmg to assert(message && 0). This will ensure that at runtime
    # executing these macros will fail producing the file and line number
    if re.match('(.*)define(.*)PRAGMA_THREAD_(.*)', line):
      print_assertMsg(line, outFileFD)
      continue

    # make sure that this not a commented line
    if re.match('(.*)//(.*)PRAGMA_THREAD_', line):
      # nothing to modify
      continue

    # If this is not an OpenMP pragma we are interested in, then  
    # spit the line as is, then continue
    if not re.match('(.*)PRAGMA_THREAD_(.*)', line):  
      outFileFD.write(line + "\n")
      continue 

    if re.match('(.*)PRAGMA_THREAD_WITHIN_A_BOX(.*)', line):  
      # Unimplemented
      outFileFD.write(line + "\n")
      continue 

    if re.match('(.*)PRAGMA_THREAD_ACROSS_BOXES(.*)', line):  
      # Unimplemented
      outFileFD.write(line + "\n")
      continue 

    # DEBUG:
    # Logs are created for each for loop modified.
    # This is just to prevent improper conversions
    # Create log header, if not already written
    if logging == 0:
      logging = 1
      logFileFD.write("============== " + inFile + " ==============\n")
    if logging == 1:
      logFileFD.write("line-" + str(lineNum) + ": " + line + "\n")  

    if re.match('(.*)PRAGMA_THREAD_ACROSS_BLOCKS(.*)', line):  
      # For every PRAGMA_THREAD_ACROSS_BLOCKS_*(level,b,nb) pragmas, we need to include the 
      # condition if(nb<=1) to enable forasync, else it will be a sequential loop
      pragma_fields = re.split(r'MyPragma', line)
      pragma_fields = re.split(r',', pragma_fields[0])
      if_var = pragma_fields[2]
      # replace if there is ")" character in the if_var
      rep = {")": ""}
      if_var = replace_all(if_var, rep)

      # Special treatment is required for macros PRAGMA_THREAD_ACROSS_BLOCKS_SUM and PRAGMA_THREAD_ACROSS_BLOCKS_MAX

      # If not a special case then assign values = "@" as below
      reductionPramga = 0
      sum_pragma_field_name = "@"
      max_pragma_field_name = "@"

      # TODO: We only consider a PRAGMA_THREAD_ACROSS_BLOCKS_SUM/_MAX valid if it has 
      # the following signature:
      # ------
      # double val =  0.0;
      # PRAGMA_THREAD_ACROSS_BLOCKS_SUM(a,b,c,val)
      #            OR
      # PRAGMA_THREAD_ACROSS_BLOCKS_MAX(a,b,c,val)
      # for(i=x;i<y;i++){
      #   double val_in = 0.0;
      #   Statements;
      #   val_in = assignments;
      #   val+=val_in; <---OR---> if(val>val_in){val = val_in;}
      # }
      #
     
      if re.match('(.*)PRAGMA_THREAD_ACROSS_BLOCKS_SUM(.*)', line):
        reductionPramga = 1
        sum_pragma_fields = re.split(r'\)', pragma_fields[3])
        sum_pragma_field_name = sum_pragma_fields[0]

      if re.match('(.*)PRAGMA_THREAD_ACROSS_BLOCKS_MAX(.*)', line):
        reductionPramga = 2
        max_pragma_fields = re.split(r'\)', pragma_fields[3])
        max_pragma_field_name = max_pragma_fields[0]

      reduction_val_name = ""
      if reductionPramga == 1:
        reduction_val_name = sum_pragma_field_name
      if reductionPramga == 2:
        reduction_val_name = max_pragma_field_name
          
      # Now proceed normally to parse the for-loop

      # increment a line number
      line = give_line(lineNum, inFileFD, dict_pos)
      lineNum = lineNum + 1

      # grep a for-loop:
      # line contains a for loop if it contains 'for', followed by '(', 
      # followed by ';' and finally ')'. Any number of characters
      # could be in between
      # TODO: Read a multiline for loop declaration
      if re.match('(.*)for(.*)\((.*);(.*)\)(.*)', line):
        line = line.rstrip()
        itrID, lowBound, highBound = parse_forloop_properties(line, inFile, lineNum)

        # We need to ensure this loop is declaring
        # its boody using braces. 
        braces, line, lineNum = ensure_forloopBody_within_braces(line, lineNum, inFileFD, dict_pos, inFile)

        # Extract the body of the for-loop
        # We run another forloop here, where, we read line
        # by line of the file for this entire for loop body.
        # Doing another loop here ensures we don't treat
        # an inner for loop as a forasync.

        # we use open and close braces ('{' and '}')
        # to mark opening and closing of a loop body
        # TODO: read body without braces
    
        # Read and print the loop/body
        if testOnly == 0:
          # Check case a-1, i.e., when loop body is defined in
          # same line as loop declaration
          # e.g., "for(i=0;i<x;i++){Statements;}
 
          # find total number of close brace on same line
          cBrace = line.count('}')
          # if (braces - cBrace) == 0:
          # one liner for-loop with body on same line ("BUT" within braces)
          
          # Extract the body of the for loop
	  loopBody, lineNum =  get_forloopBody((braces - cBrace), line, lineNum, dict_pos, inFileFD)

          # We will need to print the entire for-loop twice:
          # case-A) The sequential case, i.e., the if-condition
          # is not true;
          # case-B) The parallel case, i.e., the forasync

          # ------- First case-A) the sequential case
          # print the if-condition
          outFileFD.write("if(" + if_var + " <= 1) {\n")
          # Now print the for-loop condition
          outFileFD.write("for(" + itrID + "=" + lowBound + ";" + itrID + "<" + highBound + ";" + itrID + "++)")
          # Now print the for-loop body -- as is.
          outFileFD.write(loopBody);

          # Now close the If-condition
          outFileFD.write("}\n")
          
          # The sequential for-loop part
          seq_forloop(itrID, lowBound, highBound, loopBody, 1, outFileFD)

          # Start the else-part
          outFileFD.write("else {\n")

          # -------  Now case-B) the parallel case
          lambdaFunc = " "
          # spit the lambda function in the output file
          lambdaFunc = lambda_signature(itrID, lowBound, highBound, reduction_val_name,  outFileFD)
 
          # Now, if required, then print the for-loop body -- with modications
          if reductionPramga>0:
            modified_forloopBody = replaceReductionVariable(loopBody, reduction_val_name, inFile)
            outFileFD.write(modified_forloopBody)
          else:
            outFileFD.write(loopBody);
  
          # Call the inner function
          lambda_call(lambdaFunc, itrID, lowBound, highBound, loopBody,  outFileFD)
          # If reduction pragma was involved, reassign the values properly
          if reductionPramga == 1: #SUMMATION
            outFileFD.write("for(int _i_=0; _i_<" + HC_WORKERS + "; _i_++) {\n")
            outFileFD.write(reduction_val_name + " += _" + reduction_val_name + "_[_i_];\n")
            outFileFD.write("}\n");
          elif reductionPramga == 2: #MAXIMUM
            outFileFD.write("for(int _i_=0; _i_<" + HC_WORKERS + "; _i_++) {\n")
            outFileFD.write("if(" + reduction_val_name + " < _" + reduction_val_name + "_[_i_]) \n")
            outFileFD.write(reduction_val_name + " = _" + reduction_val_name + "_[_i_];\n") 
            outFileFD.write("}\n");
          
          # Close the else-part
          outFileFD.write("}\n")
          # Close the lambda/parallel part specific brace
          outFileFD.write("}\n")
     
        totalFors = totalFors + 1

      else:
        throw_err(inFile, lineNum, "Immediately after a PRAGMA_THREAD_ACROSS_BLOCKS, the for-loop declaration is expected")

    elif re.match('(.*)PRAGMA_THREAD_ACROSS_LOOP\((.*)', line):  
      
      # increment a line number
      line = give_line(lineNum, inFileFD, dict_pos)
      lineNum = lineNum + 1

      # grep a for-loop:
      # line contains a for loop if it contains 'for', followed by '(', 
      # followed by ';' and finally ')'. Any number of characters
      # could be in between
      # TODO: Read a multiline for loop declaration
      if not re.match('(.*)for(.*)\((.*);(.*)\)(.*)', line):
        throw_err(inFile, lineNum, "Immediately after a PRAGMA_THREAD_ACROSS_LOOP, the for-loop declaration is expected")

      line = line.rstrip()
      itrID, lowBound, highBound = parse_forloop_properties(line, inFile, lineNum)

      # We need to ensure this loop is declaring
      # its boody using braces. 
      braces, line, lineNum = ensure_forloopBody_within_braces(line, lineNum, inFileFD, dict_pos, inFile)
          
      # find total number of close brace on same line
      cBrace = line.count('}')

      # Extract the body of the for loop
      loopBody, lineNum =  get_forloopBody((braces - cBrace), line, lineNum, dict_pos, inFileFD)

      # The sequential for-loop part
      seq_forloop(itrID, lowBound, highBound, loopBody, 0, outFileFD)

      # spit the lambda function name in the output file
      lambdaFunc = " "
      lambdaFunc = lambda_signature(itrID, lowBound, highBound, "",  outFileFD)

      # Now print the for-loop body
      outFileFD.write(loopBody)

      # Call the inner function
      lambda_call(lambdaFunc, itrID, lowBound, highBound, loopBody, outFileFD)

      # Close the lambda/parallel part specific brace
      outFileFD.write("}\n")

    elif re.match('(.*)PRAGMA_THREAD_ACROSS_LOOP_COLLAPSE_(.*)', line):  
      collapse_2 = line.count('PRAGMA_THREAD_ACROSS_LOOP_COLLAPSE_2')
      collapse_3 = line.count('PRAGMA_THREAD_ACROSS_LOOP_COLLAPSE_3')
     
      # increment a line number
      line = give_line(lineNum, inFileFD, dict_pos)
      lineNum = lineNum + 1

      # grep a for-loop:
      # line contains a for loop if it contains 'for', followed by '(', 
      # followed by ';' and finally ')'. Any number of characters
      # could be in between
      # TODO: Read a multiline for loop declaration
      if not re.match('(.*)for(.*)\((.*);(.*)\)(.*)', line):
        throw_err(inFile, lineNum, "Immediately after a PRAGMA_THREAD_ACROSS_LOOP_COLLAPSE_2, the for-loop declaration is expected")

      line = line.rstrip()
      itrID1, lowBound1, highBound1 = parse_forloop_properties(line, inFile, lineNum)

      # We need to ensure this loop is declaring
      # its boody using braces. 
      braces1, line, lineNum = ensure_forloopBody_within_braces(line, lineNum, inFileFD, dict_pos, inFile)
      # Move another line to find another for-loop declaration. If not found then throw error
      # increment a line number
      line = give_line(lineNum, inFileFD, dict_pos)
      lineNum = lineNum + 1
      if not re.match('(.*)for(.*)\((.*);(.*)\)(.*)', line):
        throw_err(inFile, lineNum, "PRAGMA_THREAD_ACROSS_LOOP_COLLAPSE_2 does not found second for-loop declaration")

      # Find the 2nd loop details
      line = line.rstrip()
      itrID2, lowBound2, highBound2 = parse_forloop_properties(line, inFile, lineNum)

      # Ensure low1 and low2 are both = 0 
      if int(lowBound1) != 0 | int(lowBound2) != 0:
        throw_err(inFile, lineNum, "For PRAGMA_THREAD_ACROSS_LOOP_COLLAPSE_2, lower bound both loops should be 0")

      # We need to ensure this second loop is also declaring
      # its boody using braces. 
      braces2, line, lineNum = ensure_forloopBody_within_braces(line, lineNum, inFileFD, dict_pos, inFile)

      itrID3 = ""
      highBound3 = ""
      braces3 = 0
      # If collapse_3 then find another set of bounds
      if collapse_3 > 0:
        # Move another line to find another for-loop declaration. If not found then throw error
        # increment a line number
        line = give_line(lineNum, inFileFD, dict_pos)
        lineNum = lineNum + 1
        if not re.match('(.*)for(.*)\((.*);(.*)\)(.*)', line):
          throw_err(inFile, lineNum, "PRAGMA_THREAD_ACROSS_LOOP_COLLAPSE_3 does not found third for-loop declaration")

        # Find the 3nd loop details
        line = line.rstrip()
        itrID3, lowBound3, highBound3 = parse_forloop_properties(line, inFile, lineNum)

        # Ensure low3 = 0 
        if int(lowBound3) != 0:
          throw_err(inFile, lineNum, "For PRAGMA_THREAD_ACROSS_LOOP_COLLAPSE_3, lower bound for all 3 loops should be 0")

        # We need to ensure this third loop is also declaring
        # its boody using braces. 
        braces3, line, lineNum = ensure_forloopBody_within_braces(line, lineNum, inFileFD, dict_pos, inFile)
      
      # find total number of close brace on same line
      cBrace = line.count('}')
      # Extract the body of the inner for loop
      loopBody, lineNum =  get_forloopBody((braces1 + braces2 + braces3 - cBrace), line, lineNum, dict_pos, inFileFD)

      loopBody_top = ""
      highB = ""
      # The iterator ID will be a unique one
      newItrID = "collapse_i"

      # The forloopBody should also reset the iterators accordingly
      if collapse_3 > 0:
        loopBody_top = "{\n"
        loopBody_top = loopBody_top + "int " + itrID1 + " = (" + newItrID + "/(" + highBound2+ "*" + highBound3 + "))%" + highBound1  + ";\n"
        loopBody_top = loopBody_top + "int " + itrID2 + " = (" + newItrID + "/" + highBound3 + ")%" + highBound2  + ";\n"
        loopBody_top = loopBody_top + "int " + itrID3 + " = " + newItrID + "%" + highBound3 + ";\n"
        highB = highBound1 + "*" + highBound2 + "*" + highBound3
      else:
        # collapse_2 case
        loopBody_top = "{\n"
        loopBody_top = loopBody_top + "int " + itrID1 + " = (" + newItrID + "/" + highBound2 + ")%" + highBound1  + ";\n"
        loopBody_top = loopBody_top + "int " + itrID2 + " = " + newItrID + "%" + highBound2 + ";\n"
        highB = highBound1 + "*" + highBound2

      loopBody = loopBody_top + loopBody + "}\n"

      # The sequential for-loop part
      seq_forloop(newItrID, "0" , highB, loopBody, 0, outFileFD)

      # spit the lambda function name in the output file
      lambdaFunc = lambda_signature(newItrID, "0", highB, "",  outFileFD)
 
      # Now print the for-loop body
      outFileFD.write(loopBody)
      
      # Call the inner function
      lambda_call(lambdaFunc, newItrID, "0", highB, loopBody, outFileFD)

      # Close the lambda/parallel part specific brace
      outFileFD.write("}\n")

    else:  
      # unexpected OpenMP pragma, throw error
      throw_err(inFile, lineNum, "Unexpected OpenMP pragma -- " + line)
      outFileFD.write(line + "\n")

  inFileFD.close()
  outFileFD.close()
  if testOnly == 1:
    print inFile, ": Parsing SUCCESS, total forasyncs transformations = ",totalFors
  mv_command = "mv " + outFile + " " + inFile
  os.system(mv_command)
###########################################
### MAIN(): READ INPUT FILE ###
###########################################

if len(sys.argv) < 3:
  print "USAGE: ./scriptName.py <Top level dir to parse> <Sequential codegen ? 0 : 1>"
  throw_err(0, 0, "Invalid directory to look into")

if len(sys.argv) == 4:
  if sys.argv[3] == "1":
    testOnly = 1
  else:
    testOnly = 0
  
sequential = sys.argv[2]
userDir = str(sys.argv[1])
userDir = userDir.rstrip()

if testOnly == 0:
  if os.path.exists(outDir):
    shutil.rmtree(outDir)
  os.makedirs(outDir)

if DEBUG == 1:
  parse_file(userDir, 1)
else:
  for root, dirs, files in os.walk(userDir):
    for file in files:
      cFile = 0
      if file.endswith(".c"):
        cFile = 1
      fPath = os.path.join(root, file) 
      print "Parsing ", fPath
      parse_file(fPath, cFile)
      command_rm = "rm -rf hcupcxx"
      os.system(command_rm) 
logFileFD.close()
############ End #####################
