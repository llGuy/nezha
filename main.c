#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <math.h>

#define NUM_CHANNELS 3
#define INPUT_HEIGHT 5
#define INPUT_WIDTH 5
#define KERNEL_SIZE 3
#define PADDING_SIZE 1
#define STRIDE 1
#define POOL_SIZE 2
#define EPSILON 1e-8
#define NUM_CLASSES 10



// For a resnet, we will just store the input, run the model, and then add the input to the output

// everything above will need to become function parameters when implenting in the shaders

void batchNormLayer(double input[NUM_CHANNELS][INPUT_HEIGHT][INPUT_WIDTH], double output[NUM_CHANNELS][INPUT_HEIGHT][INPUT_WIDTH], double gamma[NUM_CHANNELS], double beta[NUM_CHANNELS]) {
    
    for (int c = 0; c < NUM_CHANNELS; c++) {
        // Calculate mean
        double mean = 0.0;
        for (int i = 0; i < INPUT_HEIGHT; i++) {
            for (int j = 0; j < INPUT_WIDTH; j++) {
                mean += input[c][i][j];
            }
        }
        mean /= (INPUT_HEIGHT * INPUT_WIDTH);
        
        // Calculate variance
        double variance = 0.0;
        for (int i = 0; i < INPUT_HEIGHT; i++) {
            for (int j = 0; j < INPUT_WIDTH; j++) {
                double diff = input[c][i][j] - mean;
                variance += diff * diff;
            }
        }
        variance /= (INPUT_HEIGHT * INPUT_WIDTH);
        
        // Calculate standard deviation
        double stddev = sqrt(variance + EPSILON);
        
        // Normalize and scale
        for (int i = 0; i < INPUT_HEIGHT; i++) {
            for (int j = 0; j < INPUT_WIDTH; j++) {
                output[c][i][j] = gamma[c] * ((input[c][i][j] - mean) / stddev) + beta[c];
            }
        }
    }
}


void maxPoolLayer(double input[NUM_CHANNELS][INPUT_HEIGHT][INPUT_WIDTH], double output[NUM_CHANNELS][(INPUT_HEIGHT + 2 * PADDING_SIZE - POOL_SIZE) / STRIDE][(INPUT_WIDTH + 2 * PADDING_SIZE - POOL_SIZE) / STRIDE]) {
    int c, i, j, m, n;

	int outputHeight = (INPUT_HEIGHT - POOL_SIZE + 2 * PADDING_SIZE) / STRIDE + 1;
    int outputWidth = (INPUT_WIDTH - POOL_SIZE + 2 * PADDING_SIZE) / STRIDE + 1;
    
    for (c = 0; c < NUM_CHANNELS; c++) {
        for (i = 0; i < outputHeight; i++) {
            for (j = 0; j < outputWidth; j++) {
                double maxVal = input[c][i * POOL_SIZE][j * POOL_SIZE];
                
                for (m = 0; m < POOL_SIZE; m++) {
                    for (n = 0; n < POOL_SIZE; n++) {
                        int inputRow = i * POOL_SIZE + m - PADDING_SIZE;
                        int inputCol = j * POOL_SIZE + n - PADDING_SIZE;
                        
                        if (inputRow >= 0 && inputRow < INPUT_HEIGHT && inputCol >= 0 && inputCol < INPUT_WIDTH) {
                            double val = input[c][inputRow][inputCol];
                            if (val > maxVal) {
                                maxVal = val;
                            }
                        }
                    }
                }
                
                output[c][i][j] = maxVal;
            }
        }
    }
}

void reluLayer(double input[NUM_CHANNELS][INPUT_HEIGHT][INPUT_WIDTH], double output[NUM_CHANNELS][INPUT_HEIGHT][INPUT_WIDTH]) {
    int c, i, j;
    
    for (c = 0; c < NUM_CHANNELS; c++) {
        for (i = 0; i < INPUT_HEIGHT; i++) {
            for (j = 0; j < INPUT_WIDTH; j++) {
                output[c][i][j] = fmax(0.0, input[c][i][j]);
            }
        }
    }
}

void convolutionalLayer(double input[][INPUT_HEIGHT][INPUT_WIDTH], double kernel[][KERNEL_SIZE][KERNEL_SIZE], double output[][(INPUT_HEIGHT - KERNEL_SIZE + 2 * PADDING_SIZE) / STRIDE + 1][(INPUT_WIDTH - KERNEL_SIZE + 2 * PADDING_SIZE) / STRIDE + 1]) {
    int c, i, j, k, l;

	int outputHeight = (INPUT_HEIGHT - KERNEL_SIZE + 2 * PADDING_SIZE) / STRIDE + 1;
    int outputWidth = (INPUT_WIDTH - KERNEL_SIZE + 2 * PADDING_SIZE) / STRIDE + 1;
    
    for (c = 0; c < NUM_CHANNELS; c++) {
        for (i = 0; i < outputHeight; i++) {
            for (j = 0; j < outputWidth; j++) { 
                double sum = 0.0;
                
                for (k = 0; k < KERNEL_SIZE; k++) {
                    for (l = 0; l < KERNEL_SIZE; l++) {
                        int inputRow = i * STRIDE + k - PADDING_SIZE;
                        int inputCol = j * STRIDE + l - PADDING_SIZE;
                        
                        if (inputRow >= 0 && inputRow < INPUT_HEIGHT && inputCol >= 0 && inputCol < INPUT_WIDTH) {
                            sum += input[c][inputRow][inputCol] * kernel[c][k][l];
                        }
                    }
                }
                
                output[c][i][j] = sum;
            }
        }
    }
}



int main (void) {
	
	// ALL LEARNABLE PARAMETERS ARE SET HERE 

	double input[NUM_CHANNELS][INPUT_HEIGHT][INPUT_WIDTH] = {
		{
			{1.0, 1.0, 1.0, 1.0, 1.0},
			{2.0, 2.0, 2.0, 2.0, 2.0},
			{3.0, 3.0, 3.0, 3.0, 3.0},
			{4.0, 4.0, 4.0, 4.0, 4.0},
			{5.0, 5.0, 5.0, 5.0, 5.0}
		},
		{
			{0.5, 0.5, 0.5, 0.5, 0.5},
			{1.5, 1.5, 1.5, 1.5, 1.5},
			{2.5, 2.5, 2.5, 2.5, 2.5},
			{3.5, 3.5, 3.5, 3.5, 3.5},
			{4.5, 4.5, 4.5, 4.5, 4.5}
		},
		{
			{2.0, 2.0, 2.0, 2.0, 2.0},
			{3.0, 3.0, 3.0, 3.0, 3.0},
			{4.0, 4.0, 4.0, 4.0, 4.0},
			{5.0, 5.0, 5.0, 5.0, 5.0},
			{6.0, 6.0, 6.0, 6.0, 6.0}
		}
	};

	double kernel[NUM_CHANNELS][KERNEL_SIZE][KERNEL_SIZE] = {
		{
			{1.0, 2.0, 1.0},
			{0.0, 0.0, 0.0},
			{-1.0, -2.0, -1.0}
		},
		{
			{-1.0, 0.0, 1.0},
			{-2.0, 0.0, 2.0},
			{-1.0, 0.0, 1.0}
		},
		{
			{0.0, 1.0, 0.0},
			{1.0, -4.0, 1.0},
			{0.0, 1.0, 0.0}
		}
	};
	
	double gamma[NUM_CHANNELS] = {1.0, 1.0, 0.5};
    double beta[NUM_CHANNELS] = {0.0, 0.0, .5};

	
	
	int outputHeight = (INPUT_HEIGHT - KERNEL_SIZE + 2 * PADDING_SIZE) / STRIDE + 1;
    int outputWidth = (INPUT_WIDTH - KERNEL_SIZE + 2 * PADDING_SIZE) / STRIDE + 1;
    double output[NUM_CHANNELS][outputHeight][outputWidth];    
    convolutionalLayer(input, kernel, output);
    
	// print array
    int c, i, j;
    for (c = 0; c < NUM_CHANNELS; c++) {
        printf("Channel %d:\n", c + 1);
        for (i = 0; i < outputHeight; i++) {
            for (j = 0; j < outputWidth; j++) {
                printf("%f ", output[c][i][j]);
            }
            printf("\n");
        }
        printf("\n");
    }
    
    return 0;
}

