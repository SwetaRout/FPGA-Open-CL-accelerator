# FPGA-Open-CL-accelerator
des.cl
This is the kernel code for encrypting the input data and is compiled in the vcl environment (ALTERA SDK).
command to compile : aoc des.cl -o bin/des.aocx

main.cpp
This is the code that launches the kernel and displays the output
command to compile : make

Compiling both the files will generate a script des and an aocx file des.aocx
Copy these files to de1SoC board and then do the following

mkdir project
source init_opencl.sh
cp /media/fat_partition/des project/
cp /media/fat_partition/des.aocx project/
cd project
aocl program /dev/acl0 des.aocx
./des

This will show display the encrypted code in both console as well as a file output.txt

To verify the result, a C++ code is written des.cpp that does the encryption using same DES algorithm and produces the same result given the input data and key are same.

Algorithm Description :
Basically, it takes a fixed length string and produces its ciphertext after a series of computations. It uses a key of 8 bytes, each with odd parity
Decryption is the same algorithm using the key in reverse order.

IP(Initial Permutation) : It is the expansion permutation done for key expansion. This is done by duplicating half of the bits : For ex: 32 bits become 48 bits.
Key mixing : This expanded key is XORed with subkeys. 
Substitution : The result is divided into 6 bit pieces. Each of the eight S-boxes replaces its six input bits with four output bits according to a non-linear transformation.In our code, these info are provided in a lookup table format in S1,S2,S3,S4,S5,S6,S7 and S8.
FP(Final Permutation) : The results are rearranged (P box)

OpenCL kernel :
Multi-core platforms helps to increase computing capacity per unit time. Hence we divide our data in chunks and encrypt them in parallel.
Our kernel code does all the substitution and gives back the cipher text. We give our input data and expanded key to the kernel.
We divide our kernel size into 8 divisions, that is, our global work size.

Results :
There is no major difference running in multi core and on CPU but the work load can be reduced. Similar method can be applied to encrypt large data and audio and video processing.

References :
http://www.scielo.edu.uy/scielo.php?script=sci_arttext&pid=S0717-50002014000100006
https://github.com/ammosh/Data-Encryption-Standard

