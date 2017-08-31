# DSIbin-inst

This project contains the binary instrumentation Pin module for DSIbin. Pin is a dynamic binary instrumentation framework from INTEL that enables the creation of dynamic program analysis tools [[1]].

This README shows how to install and use this Pin module, which is utilized by the *Data Structure Investigator (DSI)* tool to create execution traces from x86 binary code. These traces are further evaluated by [DSIbin](https://github.com/uniba-swt/DSIbin) to identify observed data structures and their relationships.

## Installation
This Pin tool has been developed with Rev 71313 (Date: Feb 03, 2015) of the Pin Framework. It is highly recommended to use this version to guarantee that the compiled Pin tool behaves as expected. The following steps show how to install the required dependencies, compile the DSIbin-inst Pin module, and setup the environment for running DSIbin. The required OS version is Ubuntu 14.04.5 LTS (Trusty Tahr); a fresh installation is assumed.

1. Install `g++-4.8`:  
`$ sudo apt-get install g++-4.8`

2. Download the PIN software from INTEL's website [[2]]:  
`$ wget http://software.intel.com/sites/landingpage/pintool/downloads/pin-3.2-81205-gcc-linux.tar.gz`

3. Extract the PIN folder from the archive:  
`$ tar -xf pin-3.2-81205-gcc-linux.tar.gz`

4. Remove the archive file:  
	`$ rm pin-3.2-81205-gcc-linux.tar.gz`

5. Propagate the path to the Pin framework by setting a corresponding environment variable with an absolute path to the Pin folder:  
`PIN_ROOT=$(pwd)/pin-3.2-81205-gcc-linux; export PIN_ROOT`  
The path propagation works properly if the following command yields the output displayed below:  
```
$ $PIN_ROOT/pin
E: Missing application name
Pin 3.2
Copyright (c) 2003-2016, Intel Corporation. All rights reserved.
VERSION: 81201 DATE: Feb  2 2017
Usage: pin [OPTION] [-t <tool> [<toolargs>]] -- <command line>
Use -help for a description of options
```

6. Enter the `DSIbin-inst` directory and call the makefile in order to compile this Pin module:  
`$ cd 'DSIbin-inst; make obj-intel/malloctrace.so`  
A file `malloctrace.so` should now be located inside the `obj-intel64` folder:
```
$ ls obj-intel64/malloctrace.so
obj-intel64/malloctrace.so
```


## Running an example
To generate a `trace.xml` from the execution of one of the test programs provided by the `DSIbin` distribution, first enter the test-programs folder of the `DSIbin` repository. In order to generate the trace, a `types.xml` file containing the type information is required. This type information is given for all test programs and has been created using the type excavator Howard [[3]] developed by [VUSec][4], which is not publicly available. For trace generation, call the supplied makefile with the folder name of a test program as the make target. Note that `sudo` is required to attach the Pin module to the process.
```
$ cd DSIbin/resources/test-programs/
$ sudo make -B mbg-dll-with-dll-children
```

Once the execution has finished, the `trace.xml` file is placed in the specified example folder. (Hint: Use `xmllint` to properly format the trace, and run `$ sudo apt-get install libxml2-utils` for installing `xmllint`. To run `xmllint`, execute `$ for xml in *xml; do xmllint --format $xml > tmp; mv tmp $xml; done` from the test program folder.)

Consult the README of the [DSIbin project](https://github.com/uniba-swt/DSIbin) for further information on how to evaluate a trace, i.e., on how to identify the data structures of the binary file under analysis.

## References
1. [Pin – A dynamic binary instrumentation tool][1]
2. [INTEL's Pin website][2]
3. [Howard: A dynamic excavator for reverse engineering data structures][3]
4. [VUSec – Systems and Network Security Group at VU Amsterdam][4]


[1]:https://software.intel.com/en-us/articles/pin-a-dynamic-binary-instrumentation-tool
[2]:https://software.intel.com/en-us/articles/pintool-downloads
[3]:https://www.isoc.org/isoc/conferences/ndss/11/pdf/5_1.pdf
[4]:https://www.vusec.net/