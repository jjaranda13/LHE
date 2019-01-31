#include "lhe.h"

#define H1_ADAPTATION                                   \
    if (hop_number<=HOP_POS_1 && hop_number>=HOP_NEG_1) \
    {                                                   \
        small_hop=true;                                 \
    } else                                              \
    {                                                   \
        small_hop=false;                                \
    }                                                   \
                                                        \
    if( (small_hop) && (last_small_hop))  {             \
        hop_1=hop_1-1;                                  \
        if (hop_1<MIN_HOP_1) {                          \
            hop_1=MIN_HOP_1;                            \
        }                                               \
                                                        \
    } else {                                            \
        hop_1=MAX_HOP_1;                                \
    }                                                   \
    last_small_hop=small_hop;
    
    
//==================================================================
// AUXILIAR FUNCTIONS
//==================================================================

/**
 * Count bits function
 */
int count_bits (int num) {
    int contador=1;
 
    while(num/10>0)
    {
        num=num/10;
        contador++;
    }

    return contador;
}

/**
 * Tme diff function
 */
double time_diff(struct timeval x , struct timeval y)
{ 
    double timediff = 1000000*(y.tv_sec - x.tv_sec) + (y.tv_usec - x.tv_usec); /* in microseconds */
     
    return timediff;
}

//==================================================================
// HUFFMAN FUNCTIONS
//==================================================================

/* 
 * Compare huffentry lengths
 */
static int huff_sym_cmp_len(const void *a, const void *b)
{
    const LheHuffEntry *aa = a, *bb = b;
    return (aa->len - bb->len)*LHE_MAX_HUFF_SIZE_SYMBOLS + aa->sym - bb->sym;
}

/* 
 * Compare huffentry lengths
 */
static int huff_mesh_cmp_len(const void *a, const void *b)
{
    const LheHuffEntry *aa = a, *bb = b;
    return (aa->len - bb->len)*LHE_MAX_HUFF_SIZE_MESH + aa->sym - bb->sym;
}

/* 
 * Compare huffentry symbols 
 */
static int huff_cmp_sym(const void *a, const void *b)
{
    const LheHuffEntry *aa = a, *bb = b;
    return aa->sym - bb->sym;
}

/**
 * Generates Huffman codes using Huffman tree
 * 
 * @param *he Huffman parameters (Huffman tree)
 * @param max_huff_size maximum number of symbols in Huffman tree
 */
int lhe_generate_huffman_codes(LheHuffEntry *he, int max_huff_size)
{
    int len, i, last;
    uint16_t code;
    uint64_t bits;
    
    bits=0;
    code = 1;
    last = max_huff_size-1;

    //Sorts Huffman table from less occurrence symbols to greater occurrence symbols
    switch (max_huff_size) 
    {
        case LHE_MAX_HUFF_SIZE_SYMBOLS:
            qsort(he, LHE_MAX_HUFF_SIZE_SYMBOLS, sizeof(*he), huff_sym_cmp_len);  
            break;
        case LHE_MAX_HUFF_SIZE_MESH:
            qsort(he, LHE_MAX_HUFF_SIZE_MESH, sizeof(*he), huff_mesh_cmp_len);  
            break;
    }

    //Deletes symbols with no occurrence in model
    while (he[last].len == 255 && last)
        last--;
    
    //Initializes code to 11...11, the length of code depends on number of symbols with occurrence in model
    //For example, if there are 6 different symbols in model (some symbols may not appear), code = 111111
    for (i=0; i<he[last].len; i++)
    {
        code|= 1 << i;
    }      
    
    //From maximum length to 0
    for (len= he[last].len; len > 0; len--) {
        //From 0 to LHE_MAX_HUFF_SIZE (all Huffman codes)
        for (i = 0; i <= last; i++) {
            //Checks if lengths are the same
            if (he[i].len == len)
            {
                //Assigns code
                he[i].code = code;
                //Substracts 1 to code. If another symbol has the same length, codes will be different
                //For example, 1111 (15), 1110 (14), 1101 (13)... all 4 bits length
                code--;
            }          
        }
        
        //Moves 1 bit to the right (Division by two)
        //For example 1111(15) - 111(7) ... 110(6)-11(3)
        code >>= 1;
    }

    //Sorts symbols from 0 to LHE_MAX_HUFF_SIZE
    qsort(he, max_huff_size, sizeof(*he), huff_cmp_sym);
    
    //Calculates number of bits
    for (i=0; i<max_huff_size; i++) 
    {
        bits += (he[i].len * he[i].count); //bits number is symbol occurrence * symbol bits
    }
    
    return bits;

}


//==================================================================
// ADVANCED LHE
// Common functions encoder and decoder 
//==================================================================

/**
 * Calculates init x, init y, final x, final y for each block of luminance and chrominance signals.
 * 
 * @param *proc Lhe processing params
 * @param block_array_Y Block parameters for luminance
 * @param block_array_UV Block parameters for chrominances
 * @param block_width_Y Block width for luminance
 * @param block_height_Y Block height for luminance
 * @param block_width_UV Block width for chrominances
 * @param block_height_UV Block height for chrominances
 * @param width_image_Y Image width for luminance
 * @param height_image_Y Image height for luminance
 * @param width_image_UV Image width for chrominances
 * @param height_image_UV Image height for chrominances 
 * @param block_x block x index
 * @param block_y block y index
 */
void lhe_calculate_block_coordinates (LheProcessing *procY, LheProcessing *procUV,
                                      uint32_t total_blocks_width, uint32_t total_blocks_height,
                                      int block_x, int block_y)
{
    uint32_t xini_Y, xfin_Y, yini_Y, yfin_Y;
    uint32_t xini_UV, xfin_UV, yini_UV, yfin_UV;
    
    //LUMINANCE
    xini_Y = block_x * procY->theoretical_block_width;
    xfin_Y = xini_Y + procY->theoretical_block_width;
      
    yini_Y = block_y * procY->theoretical_block_height;
    yfin_Y = yini_Y + procY->theoretical_block_height ;

    
    //CHROMINANCE UV
    xini_UV = block_x * procUV->theoretical_block_width;
    xfin_UV = xini_UV + procUV->theoretical_block_width;
    
    yini_UV = block_y * procUV->theoretical_block_height;
    yfin_UV = yini_UV + procUV->theoretical_block_height ;
        
    //LIMITS
    //If width cant be divided by 32, all pixel excess is in the last block
    if (block_x == total_blocks_width -1) 
    {
        xfin_Y = procY->width;
        xfin_UV = procUV->width;
    }
    
    if (block_y == total_blocks_height -1) 
    {
        yfin_Y = procY->height;
        yfin_UV = procUV->height;
    }

    procY->basic_block[block_y][block_x].x_ini = xini_Y;
    procY->basic_block[block_y][block_x].x_fin = xfin_Y;
    procY->basic_block[block_y][block_x].y_ini = yini_Y;
    procY->basic_block[block_y][block_x].y_fin = yfin_Y;

    procUV->basic_block[block_y][block_x].x_ini = xini_UV;
    procUV->basic_block[block_y][block_x].x_fin = xfin_UV;
    procUV->basic_block[block_y][block_x].y_ini = yini_UV;
    procUV->basic_block[block_y][block_x].y_fin = yfin_UV;
    
    //Block length is calculated as fin-ini. I calculated block length because it is possible there are 
    //different block sizes. For example, any image whose width cant be divided by 32(number of blocks 
    //widthwise) will have at least one block that is smaller than the others.
    procY->basic_block[block_y][block_x].block_width = xfin_Y - xini_Y;
    procY->basic_block[block_y][block_x].block_height = yfin_Y - yini_Y;
    procUV->basic_block[block_y][block_x].block_width = xfin_UV - xini_UV;
    procUV->basic_block[block_y][block_x].block_height = yfin_UV - yini_UV;

}

/**
 * Transforms perceptual relevance to pixels per pixels
 * 
 * @param *proc Lhe processing params
 * @param compression_factor compression factor 
 * @param ppp_max_theoric maximum number of pixels per pixel
 * @param block_x block x index
 * @param block_y block y index
 */
float lhe_advanced_perceptual_relevance_to_ppp (LheProcessing *procY, LheProcessing *procUV,
                                                float compression_factor, uint32_t ppp_max_theoric,
                                                int block_x, int block_y) 
{
    float const1, const2, ppp_min, ppp_max;
    float ppp_x_0, ppp_x_1, ppp_x_2, ppp_x_3, ppp_y_0, ppp_y_1, ppp_y_2, ppp_y_3;
    float perceptual_relevance_x_0, perceptual_relevance_x_1, perceptual_relevance_x_2, perceptual_relevance_x_3;
    float perceptual_relevance_y_0, perceptual_relevance_y_1, perceptual_relevance_y_2, perceptual_relevance_y_3;

    const1 = ppp_max_theoric - 1.0;
    const2 = ppp_max_theoric * compression_factor;
    
    perceptual_relevance_x_0 = procY->perceptual_relevance_x[block_y][block_x];
    perceptual_relevance_x_1 = procY->perceptual_relevance_x[block_y][block_x+1];
    perceptual_relevance_x_2 = procY->perceptual_relevance_x[block_y+1][block_x];
    perceptual_relevance_x_3 = procY->perceptual_relevance_x[block_y+1][block_x+1];
    
    perceptual_relevance_y_0 = procY->perceptual_relevance_y[block_y][block_x];
    perceptual_relevance_y_1 = procY->perceptual_relevance_y[block_y][block_x+1];
    perceptual_relevance_y_2 = procY->perceptual_relevance_y[block_y+1][block_x];
    perceptual_relevance_y_3 = procY->perceptual_relevance_y[block_y+1][block_x+1];
    
    if (perceptual_relevance_x_0 == 0) { ppp_x_0 = ppp_max_theoric; } else { ppp_x_0 = const2 / (1.0 + const1 * perceptual_relevance_x_0); if (ppp_x_0< PPP_MIN) { ppp_x_0 = PPP_MIN; } }
    if (perceptual_relevance_x_1 == 0) { ppp_x_1 = ppp_max_theoric; } else { ppp_x_1 = const2 / (1.0 + const1 * perceptual_relevance_x_1); if (ppp_x_1< PPP_MIN) { ppp_x_1 = PPP_MIN; } }     
    if (perceptual_relevance_x_2 == 0) { ppp_x_2 = ppp_max_theoric; } else { ppp_x_2 = const2 / (1.0 + const1 * perceptual_relevance_x_2); if (ppp_x_2< PPP_MIN) { ppp_x_2 = PPP_MIN; } }  
    if (perceptual_relevance_x_3 == 0) { ppp_x_3 = ppp_max_theoric; } else { ppp_x_3 = const2 / (1.0 + const1 * perceptual_relevance_x_3); if (ppp_x_3< PPP_MIN) { ppp_x_3 = PPP_MIN; } }   
    if (perceptual_relevance_y_0 == 0) { ppp_y_0 = ppp_max_theoric; } else { ppp_y_0 = const2 / (1.0 + const1 * perceptual_relevance_y_0); if (ppp_y_0< PPP_MIN) { ppp_y_0 = PPP_MIN; } }    
    if (perceptual_relevance_y_1 == 0) { ppp_y_1 = ppp_max_theoric; } else { ppp_y_1 = const2 / (1.0 + const1 * perceptual_relevance_y_1); if (ppp_y_1< PPP_MIN) { ppp_y_1 = PPP_MIN; } }   
    if (perceptual_relevance_y_2 == 0) { ppp_y_2 = ppp_max_theoric; } else { ppp_y_2 = const2 / (1.0 + const1 * perceptual_relevance_y_2); if (ppp_y_2< PPP_MIN) { ppp_y_2 = PPP_MIN; } }        
    if (perceptual_relevance_y_3 == 0) { ppp_y_3 = ppp_max_theoric; } else { ppp_y_3 = const2 / (1.0 + const1 * perceptual_relevance_y_3); if (ppp_y_3< PPP_MIN) { ppp_y_3 = PPP_MIN; } }

    //Looks for ppp_min
    ppp_min = ppp_max_theoric;
	if (ppp_x_0 < ppp_min) ppp_min = ppp_x_0;
	if (ppp_x_1 < ppp_min) ppp_min = ppp_x_1;
	if (ppp_x_2 < ppp_min) ppp_min = ppp_x_2;
	if (ppp_x_3 < ppp_min) ppp_min = ppp_x_3;
	if (ppp_y_0 < ppp_min) ppp_min = ppp_y_0;
	if (ppp_y_1 < ppp_min) ppp_min = ppp_y_1;
	if (ppp_y_2 < ppp_min) ppp_min = ppp_y_2;
	if (ppp_y_3 < ppp_min) ppp_min = ppp_y_3;

    //Max elastic restriction
    ppp_max = ppp_min * ELASTIC_MAX;
    
    if (ppp_max > ppp_max_theoric) ppp_max = ppp_max_theoric;
    
	//Adjust values
	if (ppp_x_0 > ppp_max) ppp_x_0 = ppp_max;
	if (ppp_x_1 > ppp_max) ppp_x_1 = ppp_max;
	if (ppp_x_2 > ppp_max) ppp_x_2 = ppp_max;
	if (ppp_x_3 > ppp_max) ppp_x_3 = ppp_max;
	if (ppp_y_0 > ppp_max) ppp_y_0 = ppp_max;
	if (ppp_y_1 > ppp_max) ppp_y_1 = ppp_max;
	if (ppp_y_2 > ppp_max) ppp_y_2 = ppp_max;
	if (ppp_y_3 > ppp_max) ppp_y_3 = ppp_max;
    
    procY->advanced_block[block_y][block_x].ppp_x[TOP_LEFT_CORNER] = ppp_x_0;
    procY->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER] = ppp_x_1;
    procY->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER] = ppp_x_2;
    procY->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER] = ppp_x_3;
    
    procY->advanced_block[block_y][block_x].ppp_y[TOP_LEFT_CORNER] = ppp_y_0;
    procY->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER] = ppp_y_1;
    procY->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER] = ppp_y_2;
    procY->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER] = ppp_y_3;
    
    procUV->advanced_block[block_y][block_x].ppp_x[TOP_LEFT_CORNER] = ppp_x_0;
    procUV->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER] = ppp_x_1;
    procUV->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER] = ppp_x_2;
    procUV->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER] = ppp_x_3;
    
    procUV->advanced_block[block_y][block_x].ppp_y[TOP_LEFT_CORNER] = ppp_y_0;
    procUV->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER] = ppp_y_1;
    procUV->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER] = ppp_y_2;
    procUV->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER] = ppp_y_3;

    
    return ppp_max;
}


/**
 * This function transform PPP values at corners in order to generate a rectangle when 
 * the block is downsampled.
 * 
 * However, this function does not assure that the block takes a rectangular shape when image is interpolated.
 * A rectangular downsampled block, after interpolation, generates a poligonal shape (not parallelepiped)
 * 
 *                                                                   
 *         original                down             interpolated 
 *          side_c              
 *     0  +-------+  1            +----+                    +
 *        |       |         ----> |    |   ---->     +             
 * side a |       | side b        +----+                                    
 *        |       |             rectangle                 +             
 *     2  +-------+  3                             +  
 *          side d                                  any shape
 * 
 * @param **basic_block Block basic parameters
 * @param **advanced_block Block advanced parameters
 * @param image_width Image width 
 * @param image_height Image height 
 * @param ppp_max Maximum number of pixels per pixel
 * @param block_x Block x index
 * @param block_y Block y index                                                        
 */
uint32_t lhe_advanced_ppp_side_to_rectangle_shape (LheProcessing *proc, float ppp_max, int block_x, int block_y) 
{
    float ppp_x_0, ppp_x_1, ppp_x_2, ppp_x_3, ppp_y_0, ppp_y_1, ppp_y_2, ppp_y_3, side_a, side_b, side_c, side_d, side_average, side_max, add;
    
    uint32_t downsampled_block_x, downsampled_block_y, x_fin_downsampled, y_fin_downsampled;
    uint32_t block_width, block_height, num_hops;

    num_hops = 0;
    
    //HORIZONTAL ADJUSTMENT
    ppp_x_0 = proc->advanced_block[block_y][block_x].ppp_x[TOP_LEFT_CORNER];
    ppp_x_1 = proc->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER];
    ppp_x_2 = proc->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER];
    ppp_x_3 = proc->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER];

    side_c = ppp_x_0 + ppp_x_1;
    side_d = ppp_x_2 + ppp_x_3;
    
    side_average = side_c;
    
    if (side_c != side_d) 
    {
        
        if (side_c < side_d) 
        {
            side_max = side_c; //side max is the side whose resolution is bigger and ppp summation is lower
        } 
        else 
        {
            side_max = side_d;
        }
        
        side_average=side_max;
        //side_average=(side_c+side_d)/2;
    }
    
    block_width = proc->basic_block[block_y][block_x].block_width;     
    downsampled_block_x = (2.0*block_width/ side_average) + 0.5;
    
    if (downsampled_block_x<SIDE_MIN) 
    {
        downsampled_block_x = SIDE_MIN;
    }
    
    proc->advanced_block[block_y][block_x].downsampled_x_side = downsampled_block_x;
    
    x_fin_downsampled = proc->basic_block[block_y][block_x].x_ini + downsampled_block_x;
    if (x_fin_downsampled > proc->width) 
    {
        x_fin_downsampled = proc->width;
    }
    proc->advanced_block[block_y][block_x].x_fin_downsampled = x_fin_downsampled;

    side_average=2.0*block_width/downsampled_block_x;
       
    //adjust side c
    //--------------
    if (ppp_x_0<=ppp_x_1)
    {       
        ppp_x_0=side_average*ppp_x_0/side_c;

        if (ppp_x_0<PPP_MIN) 
        {
            ppp_x_0=PPP_MIN;
            
        }//PPPmin is 1 a PPP value <1 is not possible

        add = 0;
        ppp_x_1=side_average-ppp_x_0;
        if (ppp_x_1>ppp_max) 
        {
            add=ppp_x_1-ppp_max; 
            ppp_x_1=ppp_max;       
        }

        ppp_x_0+=add;
    }
    else
    {
        ppp_x_1=side_average*ppp_x_1/side_c;

        if (ppp_x_1<PPP_MIN) 
        { 
            ppp_x_1=PPP_MIN;    
        }//PPPmin is 1 a PPP value <1 is not possible
        
        add=0;
        ppp_x_0=side_average-ppp_x_1;
        if (ppp_x_0>ppp_max) 
        {
            add=ppp_x_0-ppp_max; 
            ppp_x_0=ppp_max;          
        }

        ppp_x_1+=add;

    }

    //adjust side d
    //--------------
    if (ppp_x_2<=ppp_x_3)
    {       
        ppp_x_2=side_average*ppp_x_2/side_d;

        
        if (ppp_x_2<PPP_MIN) 
        {
            ppp_x_2=PPP_MIN;
            
        }// PPP can not be <PPP_MIN
        
        add=0;
        ppp_x_3=side_average-ppp_x_2;
        if (ppp_x_3>ppp_max) 
        {
            add=ppp_x_3-ppp_max; 
            ppp_x_3=ppp_max;
        }

        ppp_x_2+=add;
    }
    else
    {
        ppp_x_3=side_average*ppp_x_3/side_d;

        if (ppp_x_3<PPP_MIN) 
        {
            ppp_x_3=PPP_MIN;
            
        }

        add=0;
        ppp_x_2=side_average-ppp_x_3;
        if (ppp_x_2>ppp_max) 
        {
            add=ppp_x_2-ppp_max; 
            ppp_x_2=ppp_max;           
        }
        ppp_x_3+=add;

    }
    
    proc->advanced_block[block_y][block_x].ppp_x[TOP_LEFT_CORNER] = ppp_x_0;
    proc->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER] = ppp_x_1;
    proc->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER] = ppp_x_2;
    proc->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER] = ppp_x_3;

    //VERTICAL ADJUSTMENT
    ppp_y_0 = proc->advanced_block[block_y][block_x].ppp_y[TOP_LEFT_CORNER];
    ppp_y_1 = proc->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER];
    ppp_y_2 = proc->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER];
    ppp_y_3 = proc->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER];
    
    side_a = ppp_y_0 + ppp_y_2;
    side_b = ppp_y_1 + ppp_y_3;
    
    side_average = side_a;
    
    if (side_a != side_b) 
    {
        
        if (side_a < side_b) 
        {
            side_max = side_a; //side max is the side whose resolution is bigger and ppp summation is lower
        } 
        else 
        {
            side_max = side_b;
        }
        
        side_average=side_max;
        //side_average=(side_a+side_b)/2;
    }
    
    //Block height is calculated as yfin-yini. I calculated block height because it is possible there are 
    //different block sizes. 
    block_height = proc->basic_block[block_y][block_x].block_height;
    downsampled_block_y = 2.0*block_height/ side_average + 0.5;    
    
    if (downsampled_block_y<SIDE_MIN) 
    {
        downsampled_block_y = SIDE_MIN;
    }
    
    proc->advanced_block[block_y][block_x].downsampled_y_side = downsampled_block_y;
    y_fin_downsampled = proc->basic_block[block_y][block_x].y_ini + downsampled_block_y;
    if (y_fin_downsampled > proc->height)
    {
        y_fin_downsampled = proc->height;
    }
    proc->advanced_block[block_y][block_x].y_fin_downsampled = y_fin_downsampled;

    side_average=2.0*block_height/downsampled_block_y;    
    
    //adjust side a
    //--------------
    if (ppp_y_0<=ppp_y_2)
    {       
        ppp_y_0=side_average*ppp_y_0/side_a;

        if (ppp_y_0<PPP_MIN) 
        {
            ppp_y_0=PPP_MIN;
            
        }//PPPmin is 1 a PPP value <1 is not possible

        add = 0;
        ppp_y_2=side_average-ppp_y_0;
        if (ppp_y_2>ppp_max) 
        {
            add=ppp_y_2-ppp_max; 
            ppp_y_2=ppp_max;       
        }

        ppp_y_0+=add;
    }
    else
    {
        ppp_y_2=side_average*ppp_y_2/side_a;

        if (ppp_y_2<PPP_MIN) 
        { 
            ppp_y_2=PPP_MIN;    
        }//PPPmin is 1 a PPP value <1 is not possible
        
        add=0;
        ppp_y_0=side_average-ppp_y_2;
        if (ppp_y_0>ppp_max) 
        {
            add=ppp_y_0-ppp_max; 
            ppp_y_0=ppp_max;          
        }

        ppp_y_2+=add;

    }

     //adjust side b
    //--------------
    if (ppp_y_1<=ppp_y_3)
    {       
        ppp_y_1=side_average*ppp_y_1/side_b;

        
        if (ppp_y_1<PPP_MIN) 
        {
            ppp_y_1=PPP_MIN;
            
        }// PPP can not be <PPP_MIN
        
        add=0;
        ppp_y_3=side_average-ppp_y_1;
        if (ppp_y_3>ppp_max) 
        {
            add=ppp_y_3-ppp_max; 
            ppp_y_3=ppp_max;
        }

        ppp_y_1+=add;
    }
    else
    {
        ppp_y_3=side_average*ppp_y_3/side_b;

        if (ppp_y_3<PPP_MIN) 
        {
            ppp_y_3=PPP_MIN;
            
        }

        add=0;
        ppp_y_1=side_average-ppp_y_3;
        if (ppp_y_1>ppp_max) 
        {
            add=ppp_y_1-ppp_max; 
            ppp_y_1=ppp_max;           
        }
        ppp_y_3+=add;

    }
    
    proc->advanced_block[block_y][block_x].ppp_y[TOP_LEFT_CORNER] = ppp_y_0;
    proc->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER] = ppp_y_1;
    proc->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER] = ppp_y_2;
    proc->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER] = ppp_y_3;

    num_hops = downsampled_block_y*downsampled_block_x;

    return num_hops;
}


//==================================================================
// LHE PRECOMPUTATION
// Precomputation methods for both LHE encoder and decoder .
//==================================================================

/**
 * Precalculates CF (compression factor) from QL (Quality Level).
 */
static void lhe_init_compression_factor_from_ql (LheBasicPrec *prec) 
{
    float cf, cf_min, cf_max, r;
    
    const float pr_min = PR_QUANT_1;
    const float pr_max = PR_QUANT_5;
    
    for (int ppp_max = 1; ppp_max < PPP_MAX_IMAGES; ppp_max++) 
    {
        cf_min = (1.0 + (ppp_max-1.0)*pr_min) / ppp_max;
        cf_max = 1.0 + (ppp_max-1.0)*pr_max;
        r = pow (cf_max/cf_min, (1.0/99.0));
        
        for (int ql=0; ql < MAX_QL; ql ++)
        {
            cf = (1.0/ppp_max) * pow (r, (99-ql));
            prec -> compression_factor[ppp_max][ql] = cf;  
        }
    }
}
/**
 * Calculates color component value in the middle of the interval for each hop.
 * Bassically this method inits the luminance value of each hop with the intermediate 
 * value between hops frontiers.
 *
 * h0--)(-----h1----center-------------)(---------h2--------center----------------)
 */
static void lhe_init_hop_center_color_component_value (LheBasicPrec *prec, int hop0_Y, int hop1, int rmax)
{
    
    //MIDDLE VALUE LUMINANCE
    //It is calculated as: luminance_center_hop= (Y_hop + (Y_hop_ant+Y_hop_next)/2)/2;

    uint8_t hop_neg_1_Y, hop_neg_2_Y, hop_neg_3_Y, hop_neg_4_Y;
    uint8_t hop_pos_1_Y, hop_pos_2_Y, hop_pos_3_Y, hop_pos_4_Y;
      
    hop_neg_1_Y = prec -> prec_luminance[hop0_Y][rmax][hop1][HOP_NEG_1];
    hop_neg_2_Y = prec -> prec_luminance[hop0_Y][rmax][hop1][HOP_NEG_2];
    hop_neg_3_Y = prec -> prec_luminance[hop0_Y][rmax][hop1][HOP_NEG_3];
    hop_neg_4_Y = prec -> prec_luminance[hop0_Y][rmax][hop1][HOP_NEG_4];
    hop_pos_1_Y = prec -> prec_luminance[hop0_Y][rmax][hop1][HOP_POS_1];
    hop_pos_2_Y = prec -> prec_luminance[hop0_Y][rmax][hop1][HOP_POS_2];
    hop_pos_3_Y = prec -> prec_luminance[hop0_Y][rmax][hop1][HOP_POS_3];
    hop_pos_4_Y = prec -> prec_luminance[hop0_Y][rmax][hop1][HOP_POS_4];
    
    //HOP-3                   
    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_NEG_3]= (hop_neg_3_Y + (hop_neg_4_Y+hop_neg_2_Y)/2)/2;

    //HOP-2                  
    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_NEG_2]= (hop_neg_2_Y+ (hop_neg_3_Y+hop_neg_1_Y)/2)/2;

    //HOP2                   
    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_POS_2]= (hop_pos_2_Y + (hop_pos_1_Y+hop_pos_3_Y)/2)/2;

    //HOP3             
    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_POS_3]= (hop_pos_3_Y + (hop_pos_2_Y+hop_pos_4_Y)/2)/2;
}
    
/**
 * Inits precalculated luminance cache
 * Calculates color component value for each hop.
 * Final color component ( luminance or chrominance) depends on hop1
 * Color component for negative hops is calculated as: hopi_Y = hop0_Y - hi
 * Color component for positive hops is calculated as: hopi_Y = hop0_Y + hi
 * where hop0_Y is hop0 component color value 
 * and hi is the luminance distance from hop0_Y to hopi_Y
 */
static void lhe_init_hop_color_component_value (LheBasicPrec *prec, int hop0_Y, int hop1, int rmax,
                                                uint8_t hop_neg_4, uint8_t hop_neg_3, uint8_t hop_neg_2,
                                                uint8_t hop_pos_2, uint8_t hop_pos_3, uint8_t hop_pos_4)
{ 
    int hop_neg_4_Y, hop_neg_3_Y, hop_neg_2_Y, hop_neg_1_Y, hop_pos_1_Y, hop_pos_2_Y, hop_pos_3_Y, hop_pos_4_Y;
    
    //HOP -4
    
    hop_neg_4_Y = hop0_Y  - hop_neg_4; ;
    
    if (hop_neg_4_Y<=MIN_COMPONENT_VALUE) 
    { 
        hop_neg_4_Y=1;
    }
    
    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_NEG_4]= (uint8_t) hop_neg_4_Y;
    
    
    //HOP-3
    hop_neg_3_Y = hop0_Y  - hop_neg_3; 

    if (hop_neg_3_Y <= MIN_COMPONENT_VALUE) 
    {
        hop_neg_3_Y=1;
        
    }
    
    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_NEG_3]= (uint8_t) hop_neg_3_Y; 


    //HOP-2
    hop_neg_2_Y = hop0_Y  - hop_neg_2; 

    if (hop_neg_2_Y <= MIN_COMPONENT_VALUE) 
    { 
        hop_neg_2_Y=1;
    }
    
    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_NEG_2]= (uint8_t) hop_neg_2_Y;


    //HOP-1
    hop_neg_1_Y= hop0_Y-hop1;

    if (hop_neg_1_Y <= MIN_COMPONENT_VALUE) 
    {
        hop_neg_1_Y=1;
    }
    
    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_NEG_1]= (uint8_t) hop_neg_1_Y;

    //HOP0(int)
    if (hop0_Y<=MIN_COMPONENT_VALUE) 
    {
        hop0_Y=1; //null hop
    }

    if (hop0_Y>MAX_COMPONENT_VALUE) 
    {
        hop0_Y=MAX_COMPONENT_VALUE;//null hop
    }
    
    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_0]= (uint8_t) hop0_Y; //null hop
    

    //HOP1
    hop_pos_1_Y = hop0_Y + hop1;

    if (hop_pos_1_Y>MAX_COMPONENT_VALUE)
    {
        hop_pos_1_Y=MAX_COMPONENT_VALUE;
    }

    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_POS_1]= (uint8_t) hop_pos_1_Y;
    
    //HOP2
    hop_pos_2_Y = hop0_Y  + hop_pos_2; 

    if (hop_pos_2_Y>MAX_COMPONENT_VALUE) 
    {
        hop_pos_2_Y=MAX_COMPONENT_VALUE;
        
    }
    
    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_POS_2]= (uint8_t) hop_pos_2_Y; 


    //HOP3
    hop_pos_3_Y = hop0_Y  + hop_pos_3; 

    if (hop_pos_3_Y > MAX_COMPONENT_VALUE) 
    {
        hop_pos_3_Y=MAX_COMPONENT_VALUE;
    }
    
    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_POS_3]= (uint8_t) hop_pos_3_Y; 


    //HOP4
    hop_pos_4_Y = hop0_Y  + hop_pos_4; 

    if (hop_pos_4_Y>MAX_COMPONENT_VALUE) 
    {
        hop_pos_4_Y=MAX_COMPONENT_VALUE;
    }           
    
    prec-> prec_luminance[hop0_Y][rmax][hop1][HOP_POS_4]= (uint8_t) hop_pos_4_Y;
}

/**
 * Inits best hop cache
 */
static void lhe_init_best_hop(LheBasicPrec* prec, int hop0_Y, int hop_1, int r_max)
{

    int j,original_color, error, min_error;

    
    for (original_color = 0; original_color<Y_MAX_COMPONENT; original_color++) 
    {            
        error = 0;
        min_error = 256;
        
        //Positive hops computation
        //-------------------------
        if (original_color - hop0_Y>=0) 
        {                        
            for (j=HOP_0;j<=HOP_POS_4;j++) 
            {               
                error= original_color - prec -> prec_luminance[hop0_Y][r_max][hop_1][j];

                if (error<0) {
                    error=-error;
                }

                if (error<min_error) 
                {
                    prec->best_hop[r_max][hop_1][original_color][hop0_Y]=j;
                    min_error=error;
                    
                }
                else
                {
                    break;
                }
            }
        }

        //Negative hops computation
        //-------------------------
        else 
        {
            for (j=HOP_0;j>=HOP_NEG_4;j--) 
            {              
                error= original_color - prec -> prec_luminance[hop0_Y][r_max][hop_1][j];
 
                if (error<0) 
                {
                    error = -error;
                }

                if (error<min_error) {
                    prec->best_hop[r_max][hop_1][original_color][hop0_Y]=j;
                    min_error=error;
                }
                else 
                {
                    break;
                }
            }
        }
    }
}

/**
 * @deprecated
 * 
 * Inits h1 cache 
 */
static void lhe_init_h1_adaptation (LheBasicPrec* prec) 
{
    uint8_t hop_prev, hop, hop_1; 
    
    for (hop_1=1; hop_1<H1_RANGE; hop_1++) 
    {
         for (hop_prev=0; hop_prev<NUMBER_OF_HOPS; hop_prev++)
            {
                for (hop = 0; hop<NUMBER_OF_HOPS; hop++) 
                {
           
                if(hop<=HOP_POS_1 && hop>=HOP_NEG_1 && hop_prev<=HOP_POS_1 && hop_prev>=HOP_NEG_1)  {

                    prec->h1_adaptation[hop_1][hop_prev][hop] = hop_1 - 1;
                    
                    if (hop_1<MIN_HOP_1) {
                        prec->h1_adaptation[hop_1][hop_prev][hop] = MIN_HOP_1;
                    } 
                    
                } else {
                    prec->h1_adaptation[hop_1][hop_prev][hop] = MAX_HOP_1;
                }
            }
        }
    }
}


void lhe_init_cache2(LheBasicPrec *prec){
///this function pre computes the cache
///la cache es cache[hop0][h1][hop] = 10.000 bytes = 10 KB

	float maxr, minr;
	double rpos, rneg;
	int h, hop_min, hop_max;
	const float range=0.8f; //con rango menor da mas calidad pero se gastan mas bits!!!!

	maxr = 4.0f;
	minr = 1.0f;

	hop_min = 1;
	hop_max = 255 - hop_min;

	for (int hop0=0;hop0<=255;hop0++){
		for (int hop1=4; hop1<=10;hop1++) {
			//ratio for possitive hops. max ratio=3 min ratio=1
			rpos = min (maxr,pow(range*((255-hop0)/hop1),1.0f/3.0f));
			rpos = max(minr,rpos);
			 //ratio for negative hops. max ratio=3 min ratio=1
			rneg = min(maxr,pow(range*(hop0/hop1),1.0f/3.0f));
			rneg = max(minr,rneg);

			//compute hops 0,1,2,6,7,8 (hops 3,4,5 are known and dont need cache)
			// 6,7 and 8 are not needed because cache is symmetrix. they are not computed
			//-------------------------------------------------------------------
			h=(int)(hop0-hop1*rneg*rneg*rneg);
			h=min(hop_max,h);h=max(h,hop_min);
			prec->cache_hops[hop0][hop1-4][0] = 0;//(uint8_t)h;//(hop0-hop1*rneg*rneg*rneg);
			h=(int)(hop0-hop1*rneg*rneg);
			h=min(hop_max,h);h=max(h,hop_min);
			prec->cache_hops[hop0][hop1-4][1] = (uint8_t)h;//(hop0-hop1*rneg*rneg);

			h=(int)(hop0-hop1*rneg);
			h=min(hop_max,h);h=max(h,hop_min);
			prec->cache_hops[hop0][hop1-4][2] = (uint8_t)h;//(hop0-hop1*rneg);

		}//endfor hop1
	}//endfor hop0

	lhe_init_compression_factor_from_ql (prec); //Inits compression factor cache 

}


/**
 * Inits lhe cache
 */
void lhe_init_cache (LheBasicPrec *prec) 
{ 
    
    float positive_ratio, negative_ratio; //pow functions

    //NEGATIVE HOPS
    uint8_t hop_neg_4, hop_neg_3, hop_neg_2;
    
    //POSITIVE HOPS
    uint8_t hop_pos_2, hop_pos_3, hop_pos_4;
    
    const float percent_range=0.8f;//0.8 is the  80%
    const float pow_index = 1.0f/3;

    //hop0_Y is hop0 component color value
    for (int hop0_Y = 0; hop0_Y<Y_MAX_COMPONENT; hop0_Y++)
    {
        //this bucle allows computations for different values of rmax from 20 to 40. 
        //however, finally only one value (25) is used in LHE
        for (int rmax=R_MIN; rmax<R_MAX ;rmax++) 
        {
            //hop1 is the distance from hop0_Y to next hop (positive or negative)
            for (int hop1 = 1; hop1<H1_RANGE; hop1++) 
            {
                //variable declaration
                float max= rmax/10.0;// control of limits if rmax is 25 then max is 2.5f;
                              
                // r values for possitive hops  
                positive_ratio=(float)pow(percent_range*(255-hop0_Y)/(hop1), pow_index);

                if (positive_ratio>max) 
                {
                    positive_ratio=max;
                }
                
                // r' values for negative hops
                negative_ratio=(float)pow(percent_range*(hop0_Y)/(hop1), pow_index);

                if (negative_ratio>max) 
                {
                    negative_ratio=max;
                }
                
                // COMPUTATION OF HOPS
                
                //  Possitive hops luminance
                hop_pos_2 = hop1*positive_ratio;
                hop_pos_3 = hop_pos_2*positive_ratio;
                hop_pos_4 = hop_pos_3*positive_ratio;

                //Negative hops luminance                        
                hop_neg_2 = hop1*negative_ratio;
                hop_neg_3 = hop_neg_2*negative_ratio;
                hop_neg_4 = hop_neg_3*negative_ratio;

                lhe_init_hop_color_component_value (prec, hop0_Y, hop1, rmax, hop_neg_4, hop_neg_3, 
                                                    hop_neg_2, hop_pos_2, hop_pos_3, hop_pos_4);
                
                if (MIDDLE_VALUE) {
                    lhe_init_hop_center_color_component_value(prec, hop0_Y, hop1, rmax);
                }
                
                lhe_init_best_hop(prec, hop0_Y, hop1, rmax );
            }
        }
    }
    
    lhe_init_compression_factor_from_ql (prec); //Inits compression factor cache 
}

//==================================================================
// LHE VIDEO 
// Video methods for both LHE encoder and decoder .
//==================================================================
/**
 * Adapts resolution of one block from last frame and the same block of the current frame
 * 
 * @param **basic_block Block basic parameters
 * @param **advanced_block Block advanced parameters of the current frame
 * @param **last_advanced_block Block advanced parameters of the last frame
 * @param *downsampled_data Array with downsampled data from last frame
 * @param *intermediate_adapted_downsampled_data Array with y coordinate adapted
 * @param *adapted_downsampled_data Array with last frame adapted data
 * @param width image width
 * @param block_x Block x index
 * @param block_y Block y index               
 */
void mlhe_adapt_downsampled_data_resolution (LheProcessing *proc, LheImage *lhe,
                                             uint8_t *intermediate_adapted_downsampled_data, uint8_t *adapted_downsampled_data,
                                             int block_x, int block_y)
{
    uint32_t xini, xfin, yini, yfin, last_xfin, last_yfin;
    uint32_t downsampled_x_side, downsampled_y_side, last_downsampled_x_side, last_downsampled_y_side;
    uint32_t xdown, xprev_interpolated, xfin_interpolated, ydown, yprev_interpolated, yfin_interpolated;
    float step_x, step_y, xfin_interpolated_float, yfin_interpolated_float, xdown_float, ydown_float;

    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin = proc->advanced_block[block_y][block_x].x_fin_downsampled;
    yini = proc->basic_block[block_y][block_x].y_ini;
    yfin = proc->advanced_block[block_y][block_x].y_fin_downsampled;
    
    last_xfin = proc->last_advanced_block[block_y][block_x].x_fin_downsampled;
    last_yfin = proc->last_advanced_block[block_y][block_x].y_fin_downsampled;
    
    downsampled_x_side = proc->advanced_block[block_y][block_x].downsampled_x_side;
    downsampled_y_side = proc->advanced_block[block_y][block_x].downsampled_y_side;
    last_downsampled_x_side = proc->last_advanced_block[block_y][block_x].downsampled_x_side;
    last_downsampled_y_side = proc->last_advanced_block[block_y][block_x].downsampled_y_side;
    
    //Adapt horizontal resolution
    if (downsampled_x_side > last_downsampled_x_side) 
    {       
        //Nearest neighbour
        step_x = 1.0 * downsampled_x_side/last_downsampled_x_side;
        
        for (int y=yini; y<last_yfin; y++)
        {        
        
            //Interpolated x coordinates
            xprev_interpolated = xini; 
            xfin_interpolated_float= xini+step_x;

            for (int x_sc=xini; x_sc<last_xfin; x_sc++)
            {
                xfin_interpolated = xfin_interpolated_float + 0.5;            
                
                for (int i=xprev_interpolated;i < xfin_interpolated;i++)
                {
                    intermediate_adapted_downsampled_data[y*proc->width+i]=lhe->last_downsampled_image[y*proc->width+x_sc]; 
                }
                            
                xprev_interpolated=xfin_interpolated;
                xfin_interpolated_float+=step_x;   
            }//x

        }//y 
    } 
    else if (downsampled_x_side < last_downsampled_x_side)
    {
        step_x = 1.0 * last_downsampled_x_side/downsampled_x_side;
        
        //SPS
        for (int y=yini; y<last_yfin; y++)
        {        
            xdown_float=xini + step_x;

            for (int x=xini; x<xfin; x++)
            {
                xdown = xdown_float - 0.5;
                        
                intermediate_adapted_downsampled_data[y*proc->width+x]=lhe->last_downsampled_image[y*proc->width+xdown];    

                xdown_float+=step_x;
            }//x
        }//y
    } 
    else 
    {
        for (int y=yini; y<last_yfin; y++)
        {        
            for (int x=xini; x<xfin; x++)
            {                        
                intermediate_adapted_downsampled_data[y*proc->width+x]=lhe->last_downsampled_image[y*proc->width+x];
            }//x
        }//y
    }
    
    
    //Adapt vertical resolution
    if (downsampled_y_side > last_downsampled_y_side) 
    {
        //Nearest neighbour
        step_y = 1.0 * downsampled_y_side/last_downsampled_y_side;
           
        for (int x=xini; x < xfin; x++)
        {
            //Interpolated y coordinates
            yprev_interpolated = yini; 
            yfin_interpolated_float= yini+step_y;

            // bucle for horizontal scanline 
            // scans the downsampled image, pixel by pixel
            for (int y_sc=yini;y_sc<last_yfin;y_sc++)
            {            
                yfin_interpolated = yfin_interpolated_float + 0.5;  
                
                for (int i=yprev_interpolated;i < yfin_interpolated;i++)
                {
                    adapted_downsampled_data[i*proc->width+x]=intermediate_adapted_downsampled_data[y_sc*proc->width+x];
                }
          
                yprev_interpolated=yfin_interpolated;
                yfin_interpolated_float+=step_y;               
                
            }//y
        }//x
    } 
    else if (downsampled_y_side < last_downsampled_y_side) 
    {
        step_y = 1.0 * last_downsampled_y_side/downsampled_y_side;

        //SPS
        for (int x=xini; x < xfin; x++)
        {

            ydown_float=yini + step_y; 
        
            for (int y=yini; y < yfin; y++)
            {
                ydown = ydown_float - 0.5;

                adapted_downsampled_data[y*proc->width+x]=intermediate_adapted_downsampled_data[ydown*proc->width+x];  

                ydown_float+=step_y;
            }//ysc
        }//x
    }
    else 
    {   
        for (int x=xini; x < xfin; x++) 
        {
            for (int y=yini; y < yfin; y++)
            {                 
                 adapted_downsampled_data[y*proc->width+x]=intermediate_adapted_downsampled_data[y*proc->width+x]; 
            }
        }
    }
}

void mlhe_oneshot_adaptres_and_compute_delta (LheProcessing *proc, LheImage *lhe,
                                             int block_x, int block_y)
{
    
	uint32_t xini, xfin, yini, yfin;
    uint32_t downsampled_x_side, downsampled_y_side, last_downsampled_x_side, last_downsampled_y_side;
    int ratioY, ratioX;
    int y, x, yprev, xprev, delta_int;
    uint8_t *down_image, *last_down_image, *delta;




    int tramo1, tramo2, signo, dif_tramo;

    tramo1 = 52;
    tramo2 = 204;

    dif_tramo = tramo1 + (tramo2-tramo1)>>1;

    down_image = lhe->downsampled_image;
    last_down_image = lhe->last_downsampled_image;
    delta = lhe->delta;

    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin = proc->advanced_block[block_y][block_x].x_fin_downsampled;
    yini = proc->basic_block[block_y][block_x].y_ini;
    yfin = proc->advanced_block[block_y][block_x].y_fin_downsampled;
    
    downsampled_x_side = proc->advanced_block[block_y][block_x].downsampled_x_side;
    downsampled_y_side = proc->advanced_block[block_y][block_x].downsampled_y_side;
    last_downsampled_x_side = proc->last_advanced_block[block_y][block_x].downsampled_x_side;
    last_downsampled_y_side = proc->last_advanced_block[block_y][block_x].downsampled_y_side;


	ratioY = (1000*last_downsampled_y_side)/downsampled_y_side;
	ratioX = (1000*last_downsampled_x_side)/downsampled_x_side;

	for (y = yini; y < yfin; y++) {
		yprev = (((y-yini) * ratioY)/1000)+yini;

		for (x = xini; x < xfin; x++) {
			xprev = (((x-xini) * ratioX)/1000)+xini;

			//delta_int = (down_image[y * proc->width + x] - last_down_image[yprev * proc->width + xprev]) / 4 + 128;

            //BLOQUE IP PIXEL INICIAL
            if (x == xini && y == yini) {
                delta[y * proc->width + x] = down_image[y * proc->width + x];
                continue;
            }
            else delta_int = down_image[y * proc->width + x] - last_down_image[yprev * proc->width + xprev];

			signo = 1;
			if (delta_int < 0) {
				signo = -1;
				delta_int = -delta_int;
			}

			if (delta_int >= tramo2) delta_int = tramo2-1;

			if (delta_int < tramo1) {

			} else {
				delta_int = delta_int - tramo1;
				delta_int = tramo1 + delta_int/2;
			}
			delta_int = signo*delta_int+128;

			delta[y * proc->width + x] = delta_int;

		}
	}
}

void mlhe_adapt_downsampled_data_resolution2 (LheProcessing *proc, LheImage *lhe,
                                             uint8_t *intermediate_adapted_downsampled_data, uint8_t *adapted_downsampled_data,
                                             int block_x, int block_y)
{
    
    uint32_t xini, xfin, yini, yfin;
    uint32_t downsampled_x_side, downsampled_y_side, last_downsampled_x_side, last_downsampled_y_side;
    int ratioY, ratioX;
    int y, x, yprev, xprev;
    uint8_t *last_down_image;

    last_down_image = lhe->last_downsampled_image;

    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin = proc->advanced_block[block_y][block_x].x_fin_downsampled;
    yini = proc->basic_block[block_y][block_x].y_ini;
    yfin = proc->advanced_block[block_y][block_x].y_fin_downsampled;
    
    downsampled_x_side = proc->advanced_block[block_y][block_x].downsampled_x_side;
    downsampled_y_side = proc->advanced_block[block_y][block_x].downsampled_y_side;
    last_downsampled_x_side = proc->last_advanced_block[block_y][block_x].downsampled_x_side;
    last_downsampled_y_side = proc->last_advanced_block[block_y][block_x].downsampled_y_side;

	ratioY = (1000*last_downsampled_y_side)/downsampled_y_side;
	ratioX = (1000*last_downsampled_x_side)/downsampled_x_side;

	for (y = yini; y < yfin; y++) {
		yprev = (((y-yini) * ratioY)/1000)+yini;

		for (x = xini; x < xfin; x++) {
			xprev = (((x-xini) * ratioX)/1000)+xini;

			adapted_downsampled_data[y*proc->width+x] = last_down_image[yprev * proc->width + xprev];
		}
	}
}

void pr_to_movement(LheProcessing *proc)
{

    int total_blocks_width, total_blocks_height, pixels_block, i, j;

    total_blocks_width = HORIZONTAL_BLOCKS;
    pixels_block = proc->width / HORIZONTAL_BLOCKS;
    total_blocks_height = proc->height / pixels_block;

    for (i = 0; i < total_blocks_width + 1; i++){
        for (j = 0; j < total_blocks_height + 1; j++){
            proc->prx_movement[j][i] = proc->perceptual_relevance_x[j][i] - proc->last_perceptual_relevance_x[j][i];
            proc->pry_movement[j][i] = proc->perceptual_relevance_y[j][i] - proc->last_perceptual_relevance_y[j][i];
            if (proc->prx_movement[j][i] < 0) proc->prx_movement[j][i] = -proc->prx_movement[j][i];
            if (proc->pry_movement[j][i] < 0) proc->pry_movement[j][i] = -proc->pry_movement[j][i];
        }
    }

}

float get_block_movement(LheProcessing *proc, int block_x, int block_y)
{
    
    float accum_y, accum_x, accum;

    accum_x = (proc->prx_movement[block_y][block_x]+proc->prx_movement[block_y][block_x+1]+proc->prx_movement[block_y+1][block_x]+proc->prx_movement[block_y+1][block_x+1])/4;
    accum_y = (proc->pry_movement[block_y][block_x]+proc->pry_movement[block_y][block_x+1]+proc->pry_movement[block_y+1][block_x]+proc->pry_movement[block_y+1][block_x+1])/4;

    if(accum_x > accum_y)
        accum = accum_x;
    else
        accum = accum_y;

    return accum;

}