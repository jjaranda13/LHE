/*
 * LHE format definitions
 */

/**
 * @file
 * LHE format definitions.
 */

#include "libavutil/opt.h"
#include "libavutil/imgutils.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <sys/time.h>
#include "huffman.h"

//Configuration OpenMP
#define OPENMP_FLAGS 1// use always block functions //strcmp(FFMPEG_CONFIGURATION, "--extra-cflags=-fopenmp --extra-ldflags=-fopenmp")
#define CONFIG_OPENMP 1

#if (CONFIG_OPENMP)
#include <omp.h>
#endif

//LHE Pixel Format
#define LHE_YUV420 0
#define LHE_YUV422 1
#define LHE_YUV444 2

//VIDEO PARAMS
static const uint8_t mlhe_sig[4] = "MLHE";
static const uint8_t lhe_sig[3] = "LHE";

#define MLHE_TRAILER                 0x3b
#define LHE_TRAILER                  0x3b
#define MLHE_EXTENSION_INTRODUCER    0x21
#define LHE_EXTENSION_INTRODUCER     0x21
#define MLHE_IMAGE_SEPARATOR         0x2c
#define LHE_IMAGE_SEPARATOR          0x2c

//Configuration 
#define BASIC_LHE 0
#define ADVANCED_LHE 1
#define DELTA_MLHE 2
#define VIDEO 2
#define MIDDLE_VALUE false
#define LUMINANCE_FACTOR 1
#define BLOCK_WIDTH_Y 64
#define BLOCK_HEIGHT_Y 64
#define BLOCK_WIDTH_UV BLOCK_WIDTH_Y/CHROMA_FACTOR_WIDTH
#define BLOCK_HEIGHT_UV BLOCK_HEIGHT_Y/CHROMA_FACTOR_HEIGHT
#define HORIZONTAL_BLOCKS 32

//Params for precomputation
#define H1_RANGE 20
#define Y_MAX_COMPONENT 256
#define R_MIN 20
#define R_MAX 40
#define RATIO R_MAX
#define NUMBER_OF_HOPS 9
#define SIGN 2
    
//Param definitions
#define POSITIVE 0
#define NEGATIVE 1

//Color component values
#define MAX_COMPONENT_VALUE 255
#define MIN_COMPONENT_VALUE 0

//LHE params
#define MAX_HOP_1 10
#define MIN_HOP_1 4
#define START_HOP_1 (MAX_HOP_1 + MIN_HOP_1) / 2
#define PARAM_R 25

//SPS Params
#define NO_SPS_RATIO 1
#define SPS_FACTOR 5

//Perceptual Relevance Params
#define PR_LUM_DIV 4
#define PR_HMAX 4.0
#define PR_MIN 0.2
#define PR_MAX 0.5
#define PR_DIF 0.3 //PR_MAX-PR_MIN
#define PR_QUANT_0 0
#define PR_QUANT_1 0.125
#define PR_QUANT_2 0.25
#define PR_QUANT_3 0.5
#define PR_QUANT_4 0.75
#define PR_QUANT_5 1
#define CORNERS 4
#define SIDE_MIN 2
#define TOP_LEFT_CORNER 0
#define TOP_RIGHT_CORNER 1
#define BOT_LEFT_CORNER 2
#define BOT_RIGHT_CORNER 3
#define QUANT_LUM0 4
#define QUANT_LUM1 8
#define QUANT_LUM2 24
#define QUANT_LUM3 72

//Hops
#define HOP_NEG_4 0 // h-4 
#define HOP_NEG_3 1 // h-3 
#define HOP_NEG_2 2 // h-2 
#define HOP_NEG_1 3 // h-1 
#define HOP_0 4     // h0 
#define HOP_POS_1 5 // h1 
#define HOP_POS_2 6 // h2 
#define HOP_POS_3 7 // h3 
#define HOP_POS_4 8 // h4 

#define RLC1 0
#define RLC2 1
#define HUFFMAN 2

#define NO_SYMBOL 20
#define NO_INTERVAL 20 

//Huffman mesh
#define LHE_MAX_HUFF_SIZE_MESH 5
#define LHE_HUFFMAN_NODE_BITS_MESH 3
#define LHE_HUFFMAN_TABLE_BITS_MESH 2*LHE_MAX_HUFF_SIZE_MESH*LHE_HUFFMAN_NODE_BITS_MESH
#define LHE_HUFFMAN_TABLE_BYTES_MESH LHE_HUFFMAN_TABLE_BITS_MESH/8 + 1
#define LHE_HUFFMAN_NO_OCCURRENCES_MESH 7

//Huffman symbols
#define LHE_MAX_HUFF_SIZE_SYMBOLS 9
#define LHE_HUFFMAN_NODE_BITS_SYMBOLS 4
#define LHE_HUFFMAN_TABLE_BITS_SYMBOLS 2*LHE_MAX_HUFF_SIZE_SYMBOLS*LHE_HUFFMAN_NODE_BITS_SYMBOLS
#define LHE_HUFFMAN_TABLE_BYTES_SYMBOLS LHE_HUFFMAN_TABLE_BITS_SYMBOLS/8 + 1
#define LHE_HUFFMAN_NO_OCCURRENCES_SYMBOLS 15


//Mesh
#define PR_INTERVAL_0 0
#define PR_INTERVAL_1 1
#define PR_INTERVAL_2 2
#define PR_INTERVAL_3 3
#define PR_INTERVAL_4 4
#define PR_INTERVAL_BITS 3 
#define PR_MESH_BITS 2*PR_INTERVAL_BITS

//Compression
#define LHE_MODE_SIZE_BITS 2
#define PIXEL_FMT_SIZE_BITS 2
#define FIRST_COLOR_SIZE_BITS 8
#define WIDTH_SIZE_BITS 32
#define HEIGHT_SIZE_BITS 32
#define QL_SIZE_BITS 8
#define PPP_MAX_IMAGES 64 //this value allows to compress images up to 4096 px widthwise
#define PPP_MIN 1

#define MAX_RECTANGLES 100



#define PPP_MAX 4
#define ELASTIC_MAX 3
#define MAX_QL 100

#define TTL_MAX 30
#define MOV_THRESHOLD 0

//STREAMING
//#define GOP 0

#define LENGTH 64//sizeof(mask)*CHAR_BIT
#define testBit(A,k) ((A & (1UL<<(k)))>>k)

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define dif(a, b) (((a) > (b)) ? ((a)-(b)) : ((b)-(a)))

typedef struct Prot_Rectangle{
	bool active;
	bool protection;
	int xini;
	int xfin;
	int yini;
	int yfin;
} Prot_Rectangle;

typedef struct LheBasicPrec {
    uint8_t prec_luminance[Y_MAX_COMPONENT][RATIO][H1_RANGE][NUMBER_OF_HOPS]; // precomputed luminance component
    uint8_t best_hop [RATIO][H1_RANGE][Y_MAX_COMPONENT][Y_MAX_COMPONENT]; //ratio - h1 - original color - prediction
    uint8_t h1_adaptation [H1_RANGE][NUMBER_OF_HOPS][NUMBER_OF_HOPS]; //h1 adaptation cache
    float compression_factor [PPP_MAX_IMAGES][MAX_QL]; //compression factor values
    uint8_t cache_hops[Y_MAX_COMPONENT][7][3];
} LheBasicPrec; 

typedef struct LheHuffEntry {
    uint8_t  sym;
    uint8_t  len;
    uint32_t code;
    uint64_t count;
} LheHuffEntry;

/*typedef struct LheReconfParams {
    int ql;
    int skip_frames;
    Rectangle protected_rectangles[MAX_RECTANGLES];
    uint8_t down_mode_p;
    bool color;
    bool pr_metrics;
    uint8_t gop;
} LheReconfParams;
*/
typedef struct BasicLheBlock {
    uint32_t block_width;
    uint32_t block_height;
    uint32_t x_ini;
    uint32_t x_fin;
    uint32_t y_ini;
    uint32_t y_fin;
} BasicLheBlock;

typedef struct AdvancedLheBlock {
    uint32_t x_fin_downsampled;
    uint32_t y_fin_downsampled;
    uint32_t downsampled_x_side;
    uint32_t downsampled_y_side;
    float ppp_x[CORNERS];
    float ppp_y[CORNERS];
    uint8_t enc_type; //I = 0/ P = 1
    int block_ttl; //After certain number of frames in which this block is P, force I block
    //uint64_t hop_counter[9];
    //bool empty_flagY;
    //bool empty_flagU;
    //bool empty_flagV;
} AdvancedLheBlock;

typedef struct LheProcessing {
    BasicLheBlock **basic_block;
    AdvancedLheBlock **advanced_block; //Only pointer
    AdvancedLheBlock **last_advanced_block; //Only pointer
    AdvancedLheBlock **buffer_advanced_block; //Allocated memory for blocks
    AdvancedLheBlock **buffer1_advanced_block; //Allocated memory for blocks
    float **perceptual_relevance_x; //Only pointer
    float **perceptual_relevance_y; //Only pointer
    float **last_perceptual_relevance_x; //Only pointer
    float **last_perceptual_relevance_y; //Only pointer
    float **buffer_perceptual_relevance_x; //Allocated memory for PR
    float **buffer_perceptual_relevance_y; //Allocated memory for PR
    float **buffer1_perceptual_relevance_x; //Allocated memory for PR
    float **buffer1_perceptual_relevance_y; //Allocated memory for PR
    float **prx_movement; //Array of differences of PRx
    float **pry_movement; //Array of differences of PRy
    uint16_t width;
    uint16_t height;
    uint8_t pr_factor;
    uint32_t theoretical_block_width;
    uint32_t theoretical_block_height;
    uint64_t pr_quanta_counter[5];
    uint32_t num_hopsY;
    uint32_t num_hopsU;
    uint32_t num_hopsV;
} LheProcessing;

typedef struct LheImage {
    uint8_t *first_color_block;
    uint8_t *component_prediction;
    uint8_t *downsampled_image;
    uint8_t *last_downsampled_image; 
    uint8_t *downsampled_player_image;
    uint8_t *delta;
    uint8_t *hops;
    uint8_t *buffer1;
    uint8_t *buffer2;
    uint8_t *buffer3;
} LheImage;

int lhe_generate_huffman_codes(LheHuffEntry *he,  int max_huff_size);
double time_diff(struct timeval x , struct timeval y);
int count_bits (int num);

/**
 * Calculates lhe init cache
 */
void lhe_init_cache (LheBasicPrec *prec);
void lhe_init_cache2 (LheBasicPrec *prec);
/**
 * ADVANCED_LHE
 * Common functions encoder and decoder
 */
void lhe_calculate_block_coordinates (LheProcessing *procY, LheProcessing *procUV,
                                      uint32_t total_blocks_width, uint32_t total_blocks_height,
                                      int block_x, int block_y);

float lhe_advanced_perceptual_relevance_to_ppp (LheProcessing *procY, LheProcessing *procUV,
                                                float compression_factor, uint32_t ppp_max_theoric,
                                                int block_x, int block_y);

uint32_t lhe_advanced_ppp_side_to_rectangle_shape (LheProcessing *proc, float ppp_max, int block_x, int block_y);

/**
 * VIDEO LHE
 * Common functions encoder and decoder
 */
void mlhe_adapt_downsampled_data_resolution (LheProcessing *proc, LheImage *lhe,
                                             uint8_t *intermediate_adapted_downsampled_data, uint8_t *downsampled_data_adapted,
                                             int block_x, int block_y);

void mlhe_oneshot_adaptres_and_compute_delta (LheProcessing *proc, LheImage *lhe,
                                             int block_x, int block_y);

void mlhe_adapt_downsampled_data_resolution2 (LheProcessing *proc, LheImage *lhe,
                                             uint8_t *intermediate_adapted_downsampled_data, uint8_t *adapted_downsampled_data,
                                             int block_x, int block_y);

void pr_to_movement(LheProcessing *proc);
float get_block_movement(LheProcessing *proc, int block_x, int block_y);