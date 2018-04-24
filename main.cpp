#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <fstream>
#include "CL/opencl.h"
#include "CL/cl.h"
#include "AOCL_Utils.h"
#include <assert.h>
#include <stdbool.h>
#include <sys/time.h>
#define NUM 32

using namespace aocl_utils;
using namespace std;

static const uint32_t LHS[16] = {
 	0x00000000, 0x00000001, 0x00000100, 0x00000101, 0x00010000, 0x00010001,
 	0x00010100, 0x00010101, 0x01000000, 0x01000001, 0x01000100, 0x01000101,
 	0x01010000, 0x01010001, 0x01010100, 0x01010101};

static const uint32_t RHS[16] = {
 		0x00000000, 0x01000000, 0x00010000, 0x01010000, 0x00000100, 0x01000100,
 		0x00010100, 0x01010100, 0x00000001, 0x01000001, 0x00010001, 0x01010001,
 		0x00000101, 0x01000101, 0x00010101, 0x01010101,
 	};

cl_platform_id platform = NULL;
scoped_array<cl_device_id> devices;
cl_device_id device = NULL;
cl_command_queue queue = NULL;
cl_program program = NULL;
cl_uint num_devices = 0;
cl_context context = NULL;
cl_kernel kernel = NULL;
cl_mem key_buffer = NULL;
cl_mem input_buffer = NULL;
cl_mem out_buffer = NULL;
cl_event event = NULL;

typedef struct {
	uint32_t esk[32];
	uint32_t dsk[32];
} des_info;

typedef struct{
	uint8_t *file_pointer;
	long length;
}FILE_INFO;
/*
function : get_file_info
parameters : char*
action : returns file information for input and output files
*/
FILE_INFO get_file_info(char* filepath)
{
	FILE *fileptr;
	long filelen;
	fileptr = fopen(filepath, "rb");
	fseek(fileptr, 0, SEEK_END);
	filelen = ftell(fileptr); 
	rewind(fileptr);   
	uint8_t *buffer;
	buffer = (uint8_t *)malloc((filelen+1)*sizeof(uint8_t));
	fread(buffer, filelen, 1, fileptr);
	fclose(fileptr); 
	FILE_INFO fileInfo;
	fileInfo.length = filelen;
	fileInfo.file_pointer = buffer;
	return fileInfo;
};

void des_encrypt(uint32_t SK[32], uint8_t key[8]);
void cleanup();
void keyexp(des_info* k, uint8_t *key);

int main()
{
	struct timeval time_start, time_end,ktime_start,ktime_end;
	gettimeofday(&time_start, NULL);
	uint8_t input_data[NUM] = {0xEA, 0x02, 0x47, 0x14, 0xAD, 0x5C, 0x4D, 0x84,0xEA, 0x02, 0x47, 0x14, 0xAD, 0x5C, 0x4D, 0x84,0xEA, 0x02, 0x47, 0x14, 0xAD, 0x5C, 0x4D, 0x84,0xEA, 0x02, 0x47, 0x14, 0xAD, 0x5C, 0x4D, 0x84};
	uint8_t keys[24] = {0x2B, 0xD6, 0x45, 0x9F, 0x82, 0xC5, 0xB3, 0x00, 0x95, 0x2C, 0x49, 0x10, 0x48, 0x81, 0xFF, 0x48, 0x2B, 0xD6, 0x45, 0x9F, 0x82, 0xC5, 0xB3, 0x00};

	cl_int status;
	printf("Initializing OpenCL\n");
	if(!setCwdToExeDir()){return false;}
	platform = findPlatform("Altera");
	if(platform == NULL)
	{
		printf("ERROR: Unable to find Altera OpenCL platform.\n");
		return false;
	}
	devices.reset(getDevices(platform, CL_DEVICE_TYPE_ALL, &num_devices));
	device = devices[0];

	printf("Platform: %s\n", getPlatformName(platform).c_str());
	printf("Using %d device(s)\n", num_devices);
	printf("  %s\n", getDeviceName(device).c_str());

	context = clCreateContext(NULL, 1, &device, NULL, NULL, &status);
	checkError(status, "Failed to create context");

	std::string binary_file = getBoardBinaryFile("des", device);
        printf("Using AOCX: %s\n", binary_file.c_str());
        program = createProgramFromBinary(context, binary_file.c_str(), &device, 1);

	status = clBuildProgram(program, 0, NULL, "", NULL, NULL);
        checkError(status, "Failed to build program");

	queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &status);
        checkError(status, "Failed to create command queue");

	const char *kernel_name = "des";
        kernel = clCreateKernel(program, kernel_name, &status);
        checkError(status, "Failed to create kernel");

	uint8_t out_data[NUM];
	char* filename = ((char*)"test");
	FILE* fp = fopen(filename, "wb");
	if (!fp) 
	{
		fprintf(stderr, "Failed to load file.\n");
		exit(1);
	}
	fwrite(input_data, sizeof(char), NUM, fp);
	fclose(fp);

	FILE_INFO fileinfo = get_file_info(filename);
	uint8_t* input_text = fileinfo.file_pointer;
	
	des_info k,l;
	keyexp(&k, keys);	

	key_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, 32*sizeof(uint32_t), NULL, &status); 
	input_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, fileinfo.length * sizeof(uint8_t), NULL, &status);
	out_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, fileinfo.length * sizeof(uint8_t), NULL, &status);

	status = clEnqueueWriteBuffer(queue, key_buffer, CL_TRUE, 0, 32*sizeof(uint32_t), k.esk, 0, NULL, NULL);
	status = clEnqueueWriteBuffer(queue, input_buffer, CL_TRUE, 0, fileinfo.length * sizeof(uint8_t), input_text, 0, NULL, NULL);

	status = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&key_buffer);
	status = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&input_buffer);
	status = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&out_buffer);

	size_t global_item_size = fileinfo.length/8;
	size_t local_item_size = 1;
	printf("\n Launch kernel \n");
	gettimeofday(&ktime_start, NULL);
	status = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, &event);
	gettimeofday(&ktime_end, NULL);
	printf("\n KERNEL launched\n");
	clWaitForEvents(1, &event);
	clFinish(queue);

	status = clEnqueueReadBuffer(queue, out_buffer, CL_TRUE, 0, fileinfo.length * sizeof(uint8_t),out_data, 0, NULL, NULL);

	char* filename1 = ((char*)"output.txt");
        FILE* fp1 = fopen(filename1, "wb");
        if (!fp)
        {
                fprintf(stderr, "Failed to load file.\n");
                exit(1);
        }
        fwrite(out_buffer, sizeof(char), NUM, fp1);
        fclose(fp1);
	for(int j=0; j<NUM; j++)
	{
		printf("%d\n", out_data[j]);
	}

	cleanup();
	gettimeofday(&time_end, NULL);
	printf("\nExecuting kernel : %ld\n",((ktime_end.tv_sec * 1000000 + ktime_end.tv_usec) - (ktime_start.tv_sec * 1000000 + ktime_start.tv_usec)));
	printf("\nTime taken to encrypt using DES : %ld\n",((time_end.tv_sec * 1000000 + time_end.tv_usec) - (time_start.tv_sec * 1000000 + time_start.tv_usec)));
}

void cleanup()
{
	int status;
	status = clFlush(queue);
        status = clFinish(queue);
        status = clReleaseKernel(kernel);
        status = clReleaseProgram(program);
        status = clReleaseMemObject(key_buffer);
        status = clReleaseMemObject(input_buffer);
        status = clReleaseMemObject(out_buffer);
        status = clReleaseCommandQueue(queue);
        status = clReleaseContext(context);
        key_buffer = NULL;
        input_buffer = NULL;
        out_buffer = NULL;

}

void keyexp(des_info *ctx, uint8_t* key_org)
{
	int i=0;
	uint32_t *SK = ctx->esk,*DSK=ctx->dsk;
	uint8_t *key = key_org;
	uint32_t X, Y, T;
	X=((uint32_t)key[0] << 24) | ((uint32_t)key[1] << 16) | ((uint32_t)key[2] << 8) | ((uint32_t)key[3]);
	Y=((uint32_t)key[4] << 24) | ((uint32_t)key[5] << 16) | ((uint32_t)key[6] << 8) | ((uint32_t)key[7]);

	T = ((Y >> 4) ^ X) & 0x0F0F0F0F;
   	X ^= T;
   	Y ^= (T << 4);
   	T = ((Y) ^ X) & 0x10101010;
   	X ^= T;
   	Y ^= (T);

   	X = (LHS[(X) & 0xF] << 3) | (LHS[(X >> 8) & 0xF] << 2) |(LHS[(X >> 16) & 0xF] << 1) | (LHS[(X >> 24) & 0xF]) | (LHS[(X >>  5) & 0xF] << 7) | (LHS[(X >> 13) & 0xF] << 6) | (LHS[(X >> 21) & 0xF] << 5) | (LHS[(X >> 29) & 0xF] << 4);

   	Y = (RHS[(Y >>1) & 0xF] << 3) | (RHS[(Y >>  9) & 0xF] << 2) | (RHS[(Y >> 17) & 0xF] << 1) | (RHS[(Y >> 25) & 0xF]) | (RHS[(Y >>  4) & 0xF] << 7) | (RHS[(Y >> 12) & 0xF] << 6) | (RHS[(Y >> 20) & 0xF] << 5) | (RHS[(Y >> 28) & 0xF] << 4);

   	X = X&0x0FFFFFFF;
   	Y = Y&0x0FFFFFFF;

	i=0;
	while(i < 16)
	{
 		if (i < 2 || i == 8 || i == 15) {
 			X = ((X << 1) | (X >> 27)) & 0x0FFFFFFF;
 			Y = ((Y << 1) | (Y >> 27)) & 0x0FFFFFFF;
 		} 
		else 
		{
 			X = ((X << 2) | (X >> 26)) & 0x0FFFFFFF;
 			Y = ((Y << 2) | (Y >> 26)) & 0x0FFFFFFF;
 		}

 		*SK++ = ((X <<  4) & 0x24000000) | ((X << 28) & 0x10000000) | ((X << 14) & 0x08000000) | ((X << 18) & 0x02080000) | ((X <<  6) & 0x01000000) | ((X <<  9) & 0x00200000) | ((X >>  1) & 0x00100000) | ((X << 10) & 0x00040000) | ((X <<  2) & 0x00020000) | ((X >> 10) & 0x00010000) | ((Y >> 13) & 0x00002000) | ((Y >>  4) & 0x00001000) | ((Y <<  6) & 0x00000800) | ((Y >>  1) & 0x00000400) | ((Y >> 14) & 0x00000200) | ((Y      ) & 0x00000100) | ((Y >>  5) & 0x00000020) | ((Y >> 10) & 0x00000010) | ((Y >>  3) & 0x00000008) | ((Y >> 18) & 0x00000004) | ((Y >> 26) & 0x00000002) | ((Y >> 24) & 0x00000001);

 		*SK++ = ((X << 15) & 0x20000000) | ((X << 17) & 0x10000000) | ((X << 10) & 0x08000000) | ((X << 22) & 0x04000000) | ((X >>  2) & 0x02000000) | ((X <<  1) & 0x01000000) | ((X << 16) & 0x00200000) | ((X << 11) & 0x00100000) | ((X <<  3) & 0x00080000) | ((X >>  6) & 0x00040000) | ((X << 15) & 0x00020000) | ((X >>  4) & 0x00010000) | ((Y >>  2) & 0x00002000) | ((Y <<  8) & 0x00001000) | ((Y >> 14) & 0x00000808) | ((Y >>  9) & 0x00000400) | ((Y      ) & 0x00000200) | ((Y <<  7) & 0x00000100) | ((Y >>  7) & 0x00000020) | ((Y >>  3) & 0x00000011) | ((Y <<  2) & 0x00000004) | ((Y >> 21) & 0x00000002);
	 	i++; 
	 }

	 i=0;
	 while(i<32)
         {
                SK[i] = SK[30-i];
                DSK[i + 1] = SK[31 - i];
                i=i+32;
         }

}


