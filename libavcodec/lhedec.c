/*
 * LHE Basic decoder
 */

#include "bytestream.h"
#include "get_bits.h"
#include "internal.h"
#include "lhe.h"
#include "unistd.h"
    
typedef struct LheState {
    AVClass *class;  
    LheBasicPrec prec;
    AVFrame * frame;
    GetBitContext gb;
    LheProcessing procY;
    LheProcessing procUV;
    LheImage lheY;
    LheImage lheU;
    LheImage lheV;
    uint8_t chroma_factor_width;
    uint8_t chroma_factor_height;
    uint8_t lhe_mode;
    uint8_t pixel_format;
    uint8_t quality_level;
    uint32_t total_blocks_width;
    uint32_t total_blocks_height;
    uint64_t global_frames_count;
} LheState;

uint8_t *intermediate_interpolated_Y, *intermediate_interpolated_U, *intermediate_interpolated_V;
uint8_t *delta_prediction_Y_dec, *delta_prediction_U_dec, *delta_prediction_V_dec;
uint8_t *intermediate_adapted_downsampled_data_Y_dec, *intermediate_adapted_downsampled_data_U_dec, *intermediate_adapted_downsampled_data_V_dec;
uint8_t *adapted_downsampled_image_Y, *adapted_downsampled_image_U, *adapted_downsampled_image_V;
double timecount;


static void lhe_init_pixel_format (AVCodecContext *avctx, LheState *s)
{

    if (avctx->pix_fmt == AV_PIX_FMT_YUV420P)
    {
        av_log(NULL, AV_LOG_INFO, "Pix fmt 420\n");
        s->chroma_factor_width = 2;
        s->chroma_factor_height = 2;
    } else if (avctx->pix_fmt == AV_PIX_FMT_YUV422P) 
    {
        av_log(NULL, AV_LOG_INFO, "Pix fmt 422\n");
        s->chroma_factor_width = 2;
        s->chroma_factor_height = 1;
    } else if (avctx->pix_fmt == AV_PIX_FMT_YUV444P) 
    {
        av_log(NULL, AV_LOG_INFO, "Pix fmt 444\n");
        s->chroma_factor_width = 1;
        s->chroma_factor_height = 1;
    } else
    {
        avctx->pix_fmt = AV_PIX_FMT_YUV420P;
        avctx->width = 32;//1920;//512;//Lo he tenido que poner asi para que se redimensionen los arrays despues de leer el paquete
        avctx->height = 32;//1080;//384;
        av_log(NULL, AV_LOG_INFO, "Pix fmt 420 con el else\n");
        s->chroma_factor_width = 2;
        s->chroma_factor_height = 2;
    }
}

static int lhedec_free_tables(LheState *s)
{

    av_free(intermediate_interpolated_Y); 
    av_free(intermediate_interpolated_U); 
    av_free(intermediate_interpolated_V);

    av_free(delta_prediction_Y_dec);
    av_free(delta_prediction_U_dec);
    av_free(delta_prediction_V_dec);
    
    av_free(intermediate_adapted_downsampled_data_Y_dec);
    av_free(intermediate_adapted_downsampled_data_U_dec);
    av_free(intermediate_adapted_downsampled_data_V_dec);
    
    av_free(adapted_downsampled_image_Y);
    av_free(adapted_downsampled_image_U);
    av_free(adapted_downsampled_image_V);

    av_free((&s->lheY)->first_color_block);
    av_free((&s->lheU)->first_color_block);
    av_free((&s->lheV)->first_color_block);

    for (int i=0; i < s->total_blocks_height; i++)
    {
        av_free((&s->procY)->last_advanced_block[i]);
    }   

    av_free((&s->procY)->last_advanced_block);
        
    for (int i=0; i < s->total_blocks_height; i++)
    {
        av_free((&s->procUV)->last_advanced_block[i]);
    }
    
    av_free((&s->procUV)->last_advanced_block);
    
    av_free((&s->lheY)->last_downsampled_image);
    av_free((&s->lheU)->last_downsampled_image);
    av_free((&s->lheV)->last_downsampled_image);

    av_free((&s->lheY)->hops);
    av_free((&s->lheU)->hops);
    av_free((&s->lheV)->hops);
        
    for (int i=0; i < s->total_blocks_height; i++)
    {
        av_free((&s->procY)->basic_block[i]);
    }
    av_free((&s->procY)->basic_block);
    
    for (int i=0; i < s->total_blocks_height; i++)
    {
        av_free((&s->procUV)->basic_block[i]);
    }
    av_free((&s->procUV)->basic_block);

    for (int i=0; i<s->total_blocks_height+1; i++) 
    {
        av_free((&s->procY)->perceptual_relevance_x[i]);
    }
    av_free((&s->procY)->perceptual_relevance_x);
    
    for (int i=0; i<s->total_blocks_height+1; i++) 
    {
        av_free((&s->procY)->perceptual_relevance_y[i]);
    }  
    av_free((&s->procY)->perceptual_relevance_y);
    
    for (int i=0; i < s->total_blocks_height; i++)
    {
        av_free((&s->procY)->advanced_block[i]);
    }
    av_free((&s->procY)->advanced_block);
    
    for (int i=0; i < s->total_blocks_height; i++)
    {
        av_free((&s->procUV)->advanced_block[i]);
    }
    av_free((&s->procUV)->advanced_block);

    av_free((&s->lheY)-> downsampled_image);
	av_free((&s->lheU)-> downsampled_image);
	av_free((&s->lheV)-> downsampled_image);

    return 0;
}

static int lhedec_alloc_tables(AVCodecContext *ctx, LheState *s)
{
    
    uint32_t pixels_block, image_size_Y, image_size_UV;
    
    image_size_Y = (&s->procY)->width * (&s->procY)->height;        
    image_size_UV = (&s->procUV)->width * (&s->procUV)->height;

    s->total_blocks_width = HORIZONTAL_BLOCKS;
    pixels_block = (&s->procY)->width / HORIZONTAL_BLOCKS;
    s->total_blocks_height = (&s->procY)->height / pixels_block;

    FF_ALLOC_ARRAY_OR_GOTO(s, intermediate_interpolated_Y, image_size_Y, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, intermediate_interpolated_U, image_size_UV, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, intermediate_interpolated_V, image_size_UV, sizeof(uint8_t), fail);

    FF_ALLOC_ARRAY_OR_GOTO(s, delta_prediction_Y_dec, image_size_Y, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, delta_prediction_U_dec, image_size_UV, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, delta_prediction_V_dec, image_size_UV, sizeof(uint8_t), fail);

    FF_ALLOC_ARRAY_OR_GOTO(s, intermediate_adapted_downsampled_data_Y_dec, image_size_Y, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, intermediate_adapted_downsampled_data_U_dec, image_size_UV, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, intermediate_adapted_downsampled_data_V_dec, image_size_UV, sizeof(uint8_t), fail);

    FF_ALLOC_ARRAY_OR_GOTO(s, adapted_downsampled_image_Y, image_size_Y, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, adapted_downsampled_image_U, image_size_UV, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, adapted_downsampled_image_V, image_size_UV, sizeof(uint8_t), fail);

    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->first_color_block, image_size_Y, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->first_color_block, image_size_UV, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->first_color_block, image_size_UV, sizeof(uint8_t), fail);

    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->last_advanced_block, s->total_blocks_height, sizeof(AdvancedLheBlock *), fail); 
        
    for (int i=0; i < s->total_blocks_height; i++)
    {
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->last_advanced_block[i], s->total_blocks_width, sizeof(AdvancedLheBlock), fail); 
    }   

    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procUV)->last_advanced_block, s->total_blocks_height, sizeof(AdvancedLheBlock *), fail); 
        
    for (int i=0; i < s->total_blocks_height; i++)
    {
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procUV)->last_advanced_block[i], s->total_blocks_width, sizeof(AdvancedLheBlock), fail); 
    }   

    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->last_downsampled_image, image_size_Y, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->last_downsampled_image, image_size_UV, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->last_downsampled_image, image_size_UV, sizeof(uint8_t), fail); 

    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->hops, image_size_Y, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->hops, image_size_UV, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->hops, image_size_UV, sizeof(uint8_t), fail);
        
    (&s->procY)->basic_block = av_calloc(s->total_blocks_height, sizeof(BasicLheBlock *));
        
    for (int i=0; i < s->total_blocks_height; i++)
    {
        (&s->procY)->basic_block[i] = av_calloc (s->total_blocks_width, sizeof(BasicLheBlock));
    }
    
    (&s->procUV)->basic_block = av_calloc(s->total_blocks_height, sizeof(BasicLheBlock *));
    
    for (int i=0; i < s->total_blocks_height; i++)
    {
        (&s->procUV)->basic_block[i] = av_calloc (s->total_blocks_width, sizeof(BasicLheBlock));
    }

    (&s->procY)->perceptual_relevance_x = av_calloc(s->total_blocks_height+1, sizeof(float*));  
    
    for (int i=0; i<s->total_blocks_height+1; i++) 
    {
        (&s->procY)->perceptual_relevance_x[i] = av_calloc(s->total_blocks_width+1, sizeof(float));
    }
    
    (&s->procY)->perceptual_relevance_y = av_calloc(s->total_blocks_height+1, sizeof(float*)); 
    
    for (int i=0; i<s->total_blocks_height+1; i++) 
    {
        (&s->procY)->perceptual_relevance_y[i] = av_calloc(s->total_blocks_width+1, sizeof(float));
    }   

    (&s->procY)->advanced_block = av_calloc(s->total_blocks_height, sizeof(AdvancedLheBlock *));
    
    for (int i=0; i < s->total_blocks_height; i++)
    {
        (&s->procY)->advanced_block[i] = av_calloc (s->total_blocks_width, sizeof(AdvancedLheBlock));
    }
    
    (&s->procUV)->advanced_block = av_calloc(s->total_blocks_height, sizeof(AdvancedLheBlock *));
    
    for (int i=0; i < s->total_blocks_height; i++)
    {
        (&s->procUV)->advanced_block[i] = av_calloc (s->total_blocks_width, sizeof(AdvancedLheBlock));
    }

    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->downsampled_image, image_size_Y, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->downsampled_image, image_size_UV, sizeof(uint8_t), fail); 
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->downsampled_image, image_size_UV, sizeof(uint8_t), fail);    
        
    return 0;

    fail:
        lhedec_free_tables(s);
        return AVERROR(ENOMEM);

}

static av_cold int lhe_decode_init(AVCodecContext *avctx)
{

    LheState *s = avctx->priv_data;

    lhe_init_pixel_format (avctx, s);

    //Initialize sizes of images
    (&s->procY)->width  = avctx->width;
    (&s->procY)->height = avctx->height;
    (&s->procUV)->width = ((&s->procY)->width - 1)/s->chroma_factor_width + 1;
    (&s->procUV)->height = ((&s->procY)->height - 1)/s->chroma_factor_height + 1;

    lhedec_alloc_tables(avctx, s);

    s->frame = av_frame_alloc();
    if (!s->frame)
        return AVERROR(ENOMEM);
    
    //lhe_init_cache(&s->prec);
    lhe_init_cache2(&s->prec);
    
    s->global_frames_count = 0;

    timecount = 0;
    
    return 0;
}

//==================================================================
// HUFFMAN FUNCTIONS
//==================================================================
/**
 * Reads Huffman table
 * 
 * @param *s LHE State
 * @param *he LHE Huffman Entry
 * @param max_huff_size Maximum number of symbols in Huffman tree
 * @param max_huff_node_bits Number of bits for each entry in the file
 * @param huff_no_occurrences Code for no occurrences
 */
static void lhe_read_huffman_table (LheState *s, LheHuffEntry *he, 
                                    int max_huff_size, int max_huff_node_bits, 
                                    int huff_no_occurrences) 
{   
    int i;
    uint8_t len;

    
    for (i=0; i< max_huff_size; i++) 
    {
        len = get_bits(&s->gb, max_huff_node_bits); 

        if (len==huff_no_occurrences) len=255; //If symbol does not have any occurence, encoder assigns 255 length. This is 15 or 7 in file
        he[i].len = len;
        he[i].sym = i; 
        he[i].code = 1024;
    }    
    
    lhe_generate_huffman_codes(he, max_huff_size);
       
}

/**
 * Translates Huffman into symbols (hops)
 * 
 * @huffman_symbol huffman symbol
 * @he Huffman entry, Huffman parameters
 * @count_bits Number of bits of huffman symbol 
 */


/**
 * Translates Huffman symbol into PR interval
 * 
 * @param huffman_symbol huffman symbol extracted from file
 * @param *he Huffman entry, Huffman params 
 * @param count_bits Number of bits of Huffman symbol
 */
static uint8_t lhe_translate_huffman_into_interval (uint32_t huffman_symbol, LheHuffEntry *he, uint8_t count_bits) 
{
    uint8_t interval;
        
    interval = NO_SYMBOL;
    
    if (huffman_symbol == he[PR_INTERVAL_0].code && he[PR_INTERVAL_0].len == count_bits)
    {
        interval = PR_INTERVAL_0;
    } 
    else if (huffman_symbol == he[PR_INTERVAL_1].code && he[PR_INTERVAL_1].len == count_bits)
    {
        interval = PR_INTERVAL_1;
    } 
    else if (huffman_symbol == he[PR_INTERVAL_2].code && he[PR_INTERVAL_2].len == count_bits)
    {
        interval = PR_INTERVAL_2;
    } 
    else if (huffman_symbol == he[PR_INTERVAL_3].code && he[PR_INTERVAL_3].len == count_bits)
    {
        interval = PR_INTERVAL_3;
    } 
    else if (huffman_symbol == he[PR_INTERVAL_4].code && he[PR_INTERVAL_4].len == count_bits)
    {
        interval = PR_INTERVAL_4;
    }
    
    
    return interval;
    
}

//==================================================================
// BASIC LHE FILE
//==================================================================
/**
 * Reads file symbols of basic lhe file
 * 
 * @param s Lhe parameters
 * @param he LHE Huffman entry
 * @param image_size image size
 * @param *symbols Symbols read from file
 */


//==================================================================
// ADVANCED LHE FILE
//==================================================================
/**
 * Reads file symbols from advanced lhe file
 * 
 * @param *s Lhe parameters
 * @param *he Huffman entry, Huffman parameters
 * @param *proc LHE processing parameters
 * @param symbols symbols array (hops)
 * @param block_x block x index
 * @param block_y block y index
 */


static inline void add_hop0(LheProcessing *proc, uint8_t *symbols, int *pix, int count) {

    for (int i = 0; i < count; i++) {
        symbols[*pix] = HOP_0;
        *pix= (*pix +1);
    }
}

static void lhe_advanced_read_file_symbols3 (LheState *s, LheProcessing *proc, uint8_t *symbols, int block_y_ini, int block_y_fin, int horizontal_blocks, int lhe_mode, int channel) {

    int xini, yini, xfin_downsampled, yfin_downsampled, pix, dif_pix;

    int mode = HUFFMAN, h0_counter = 0, hops_counter = 0, hop = 15, rlc_number = 0, ahorro = 0;
    int condition_length = 7;
    int rlc_length = 4;
    uint32_t num_hops;
    unsigned int data = 3;
    int a;

    int block_x = block_y_ini*horizontal_blocks;
    int inc_x = 1;
 
    uint8_t *hops = av_calloc(proc->width*proc->height, sizeof(uint8_t));

    struct timeval before , after;

    if (channel == 0) {num_hops = proc->num_hopsY;}
    else if (channel == 1) {num_hops = proc->num_hopsU;}
    else if (channel == 2) {num_hops = proc->num_hopsV;}

    gettimeofday(&before , NULL);

    while (true){
        //av_log(NULL, AV_LOG_INFO, "hops_counter: %d\n", hops_counter);
        if (hops_counter == num_hops) break;
        data = show_bits(&s->gb, 8);
        switch (mode){
            case HUFFMAN:
                if (ahorro == 0) {
                    if (data >= 128) {
                        hop = HOP_0;
                        skip_bits(&s->gb, 1);
                    } else if (data >= 64) {
                        hop = HOP_POS_1;
                        skip_bits(&s->gb, 2);
                    } else if (data >= 32) {
                        hop = HOP_NEG_1;
                        skip_bits(&s->gb, 3);
                    } else if (data > 3) {
                        if (data >= 16) {
                            hop = HOP_POS_2;
                            skip_bits(&s->gb, 4);
                        } else if (data >= 8) {
                            hop = HOP_NEG_2;
                            skip_bits(&s->gb, 5);
                        } else {
                            hop = HOP_POS_3;
                            skip_bits(&s->gb, 6);
                        }
                    } else {
                        if (data >= 2) {
                            hop = HOP_NEG_3;
                            skip_bits(&s->gb, 7);
                        } else if (data >= 1) {
                            hop = HOP_POS_4;
                            skip_bits(&s->gb, 8);
                        } else {
                            hop = HOP_NEG_4;
                            skip_bits(&s->gb, 8);
                        }
                    }
                } else {
                    if (data >= 128) {
                        hop = HOP_POS_1;
                        skip_bits(&s->gb, 1);
                    } else if (data >= 64) {
                        hop = HOP_NEG_1;
                        skip_bits(&s->gb, 2);
                    } else if (data >= 32) {
                        hop = HOP_POS_2;
                        skip_bits(&s->gb, 3);
                    } else if (data > 3) {
                        if (data >= 16) {
                            hop = HOP_NEG_2;
                            skip_bits(&s->gb, 4);
                        } else if (data >= 8) {
                            hop = HOP_POS_3;
                            skip_bits(&s->gb, 5);
                        } else {
                            hop = HOP_NEG_3;
                            skip_bits(&s->gb, 6);
                        }
                    } else {
                        if (data >= 2) {
                            hop = HOP_POS_4;
                            skip_bits(&s->gb, 7);
                        } else {
                            hop = HOP_NEG_4;
                            skip_bits(&s->gb, 7);
                        }
                    }
                } 

                ahorro=0;
                if (hop == HOP_0) h0_counter++;
                else h0_counter = 0;
                if (h0_counter == condition_length) mode = RLC1;
                hops[hops_counter] = hop;
                hops_counter++;
            break;
            case RLC1:
                data = get_bits(&s->gb, 1);
                if (data == 0) {
                    rlc_number = get_bits(&s->gb, rlc_length);
                    add_hop0(proc, hops, &hops_counter, rlc_number);
                    h0_counter = 0;
                    mode = HUFFMAN;
                    ahorro=1;

                } else {
                    add_hop0(proc, hops, &hops_counter, 15);
                    rlc_length += 1;
                    mode = RLC2;
                }
            break;
            case RLC2:
                data = get_bits(&s->gb, 1);
                if (data == 0) {
                    rlc_number = get_bits(&s->gb, rlc_length);
                    add_hop0(proc, hops, &hops_counter, rlc_number);
                    rlc_length = 4;
                    h0_counter = 0;
                    mode = HUFFMAN;
                    ahorro=1;
                } else {
                    add_hop0(proc, hops, &hops_counter, 31);
                }
            break;
        }
    }

    gettimeofday(&after , NULL);
    timecount = time_diff(before , after);
    //av_log(NULL, AV_LOG_PANIC, "Tiempo en procesar los bits: %d\n", timecount);

    a = 0;

    gettimeofday(&before , NULL);

    for (int block_y = block_y_ini; block_y < block_y_fin; block_y++) {
        for (int i = 0; i < horizontal_blocks; i++) {

            xini = proc->basic_block[block_y][block_x].x_ini;
            yini = proc->basic_block[block_y][block_x].y_ini;
            
            if (lhe_mode != BASIC_LHE) {
                xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled;          
                yfin_downsampled = proc->advanced_block[block_y][block_x].y_fin_downsampled;

                pix = yini*proc->width+xini;
                dif_pix = proc->width-xfin_downsampled+xini;
            } else {
                xfin_downsampled = proc->width;          
                yfin_downsampled = proc->height;

                pix = 0;
                dif_pix = 0;
            }

            for (int y = yini; y < yfin_downsampled; y++) {
                for (int x = xini; x < xfin_downsampled; x++) {
                    symbols[pix] = hops[a];
                    a++;
                    pix++;
                }
                pix+=dif_pix;
            }

            block_x += inc_x;
        }
        block_x=0;
    }

    gettimeofday(&after , NULL);
    timecount = time_diff(before , after);
    //av_log(NULL, AV_LOG_WARNING, "Tiempo en procesar los hops: %d\n", timecount);

    av_free(hops);
}







/**
 * Reads file symbols from advanced lhe file
 * 
 * @param *s Lhe parameters
 * @param *he_Y Luminance Huffman entry, Huffman parameters
 * @param *he_UV Chrominance Huffman entry, Huffman parameters
 */
static void lhe_advanced_read_all_file_symbols (LheState *s) 
{
    LheProcessing *procY, *procUV;
    LheImage *lheY, *lheU, *lheV;
    
    procY = &s->procY;
    procUV = &s->procUV;
    
    lheY = &s->lheY;
    lheU = &s->lheU;
    lheV = &s->lheV;
    
    lhe_advanced_read_file_symbols3 (s, procY, lheY->hops, 0, s->total_blocks_height, HORIZONTAL_BLOCKS, ADVANCED_LHE, 0);
    lhe_advanced_read_file_symbols3 (s, procUV, lheU->hops, 0, s->total_blocks_height, HORIZONTAL_BLOCKS, ADVANCED_LHE, 1);
    lhe_advanced_read_file_symbols3 (s, procUV, lheV->hops, 0, s->total_blocks_height, HORIZONTAL_BLOCKS, ADVANCED_LHE, 2);

}

/**
 * Translates perceptual relevance intervals to perceptual relevance quants
 * 
 *    Interval   -  Quant - Interval number
 * [0.0, 0.125)  -  0.0   -         0
 * [0.125, 0.25) -  0.125 -         1
 * [0.25, 0.5)   -  0.25  -         2
 * [0.5, 0.75)   -  0.5   -         3
 * [0.75, 1]     -  1.0   -         4
 * 
 * @param perceptual_relevance_interval perceptual relevance interval number
 */
static float lhe_advance_translate_pr_interval_to_pr_quant (uint8_t perceptual_relevance_interval)
{
    float perceptual_relevance_quant;
    
    switch (perceptual_relevance_interval) 
    {
        case PR_INTERVAL_0:
            perceptual_relevance_quant = PR_QUANT_0;
            break;
        case PR_INTERVAL_1:
            perceptual_relevance_quant = PR_QUANT_1;
            break;
        case PR_INTERVAL_2:
            perceptual_relevance_quant = PR_QUANT_2;
            break;
        case PR_INTERVAL_3:
            perceptual_relevance_quant = PR_QUANT_3;
            break;
        case PR_INTERVAL_4:
            perceptual_relevance_quant = PR_QUANT_5;
            break;     
    }
    
    return perceptual_relevance_quant;
}

/**
 * Reads Perceptual Relevance interval values from file
 * 
 * @param *s Lhe params
 * @param *he_mesh Huffman params for LHE mesh
 * @param **perceptual_relevance Perceptual Relevance values
 */
static void lhe_advanced_read_perceptual_relevance_interval (LheState *s, LheHuffEntry *he_mesh, float ** perceptual_relevance) 
{
    uint8_t perceptual_relevance_interval, count_bits;
    uint32_t huffman_symbol;
    
    perceptual_relevance_interval = NO_INTERVAL;
    count_bits = 0;
    huffman_symbol = 0;
    
    for (int block_y=0; block_y<s->total_blocks_height+1; block_y++) 
    {
        for (int block_x=0; block_x<s->total_blocks_width+1;) 
        { 
            //Reads from file
            huffman_symbol = (huffman_symbol<<1) | get_bits(&s->gb, 1);
            count_bits++;
            
            perceptual_relevance_interval = lhe_translate_huffman_into_interval(huffman_symbol, he_mesh, count_bits);        
            
            if (perceptual_relevance_interval != NO_INTERVAL) 
            {
                perceptual_relevance[block_y][block_x] = lhe_advance_translate_pr_interval_to_pr_quant(perceptual_relevance_interval);;
                block_x++;
                huffman_symbol = 0;
                count_bits = 0;
            }                  
        }
    }
}

/**
 * Reads perceptual intervals and translates them to perceptual relevance quants.
 * Calculates block coordinates according to perceptual relevance values.
 * Calculates pixels per pixel according to perceptual relevance values.
 * 
 * @param *s Lhe params
 * @param *he_mesh Huffman params for LHE mesh
 * @param ppp_max_theoric Maximum number of pixels per pixel
 * @param compression_factor Compression factor number
 */
static void lhe_advanced_read_mesh (LheState *s, LheHuffEntry *he_mesh, float ppp_max_theoric, float compression_factor) 
{
    LheProcessing *procY, *procUV;   
    procY = &s->procY;
    procUV = &s->procUV;

    procY->num_hopsY = 0;
    procUV->num_hopsU = 0;
    procUV->num_hopsV = 0;

    lhe_advanced_read_perceptual_relevance_interval (s, he_mesh, procY->perceptual_relevance_x);
    
    lhe_advanced_read_perceptual_relevance_interval (s, he_mesh, procY->perceptual_relevance_y);
    
    
    for (int block_y=0; block_y<s->total_blocks_height; block_y++)
    {
        for (int block_x=0; block_x<s->total_blocks_width; block_x++)
        {
            lhe_calculate_block_coordinates (procY, procUV, s->total_blocks_width, s->total_blocks_height,
                                             block_x, block_y);

            lhe_advanced_perceptual_relevance_to_ppp(procY, procUV, compression_factor, ppp_max_theoric, block_x, block_y);
        }
    }

    for (int block_y=0; block_y<s->total_blocks_height; block_y++)
    {
        for (int block_x=0; block_x<s->total_blocks_width; block_x++)
        {
            //Adjusts luminance ppp to rectangle shape 
            procY->num_hopsY += lhe_advanced_ppp_side_to_rectangle_shape (procY, ppp_max_theoric, block_x, block_y);  
            //Adjusts chrominance ppp to rectangle shape
            procUV->num_hopsU += lhe_advanced_ppp_side_to_rectangle_shape (procUV, ppp_max_theoric, block_x, block_y);
            procUV->num_hopsV = procUV->num_hopsU;

            //Ajuste de adyacencias junio 2018
            if (block_x < s->total_blocks_width-1 && block_y < s->total_blocks_height-1){
                
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y][block_x+1].ppp_x[TOP_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y][block_x+1].ppp_x[TOP_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y][block_x+1].ppp_x[TOP_LEFT_CORNER])/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y][block_x+1].ppp_y[TOP_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y][block_x+1].ppp_y[TOP_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y][block_x+1].ppp_y[TOP_LEFT_CORNER])/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y][block_x+1].ppp_x[BOT_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y][block_x+1].ppp_x[BOT_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y][block_x+1].ppp_x[BOT_LEFT_CORNER])/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y][block_x+1].ppp_y[BOT_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y][block_x+1].ppp_y[BOT_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y][block_x+1].ppp_y[BOT_LEFT_CORNER])/2;

                if ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER] < (&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER]+(&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_LEFT_CORNER] )/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_RIGHT_CORNER])
                    (&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_RIGHT_CORNER] =  ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_RIGHT_CORNER])/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER] < (&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_LEFT_CORNER] = ( (&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER]+(&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_LEFT_CORNER] )/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_RIGHT_CORNER])
                    (&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_RIGHT_CORNER] =  ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_RIGHT_CORNER])/2;

                if ((&s->procUV)->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER] < (&s->procUV)->advanced_block[block_y][block_x+1].ppp_x[TOP_LEFT_CORNER])
                    (&s->procUV)->advanced_block[block_y][block_x+1].ppp_x[TOP_LEFT_CORNER] = ((&s->procUV)->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER]+(&s->procUV)->advanced_block[block_y][block_x+1].ppp_x[TOP_LEFT_CORNER])/2;
                if ((&s->procUV)->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER] < (&s->procUV)->advanced_block[block_y][block_x+1].ppp_y[TOP_LEFT_CORNER])
                    (&s->procUV)->advanced_block[block_y][block_x+1].ppp_y[TOP_LEFT_CORNER] = ((&s->procUV)->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER]+(&s->procUV)->advanced_block[block_y][block_x+1].ppp_y[TOP_LEFT_CORNER])/2;
                if ((&s->procUV)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER] < (&s->procUV)->advanced_block[block_y][block_x+1].ppp_x[BOT_LEFT_CORNER])
                    (&s->procUV)->advanced_block[block_y][block_x+1].ppp_x[BOT_LEFT_CORNER] = ((&s->procUV)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER]+(&s->procUV)->advanced_block[block_y][block_x+1].ppp_x[BOT_LEFT_CORNER])/2;
                if ((&s->procUV)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER] < (&s->procUV)->advanced_block[block_y][block_x+1].ppp_y[BOT_LEFT_CORNER])
                    (&s->procUV)->advanced_block[block_y][block_x+1].ppp_y[BOT_LEFT_CORNER] = ((&s->procUV)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER]+(&s->procUV)->advanced_block[block_y][block_x+1].ppp_y[BOT_LEFT_CORNER])/2;
                
                if ((&s->procUV)->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER] < (&s->procUV)->advanced_block[block_y+1][block_x].ppp_x[TOP_LEFT_CORNER])
                    (&s->procUV)->advanced_block[block_y+1][block_x].ppp_x[TOP_LEFT_CORNER] =  ((&s->procUV)->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER]+(&s->procUV)->advanced_block[block_y+1][block_x].ppp_x[TOP_LEFT_CORNER] )/2;
                if ((&s->procUV)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER] < (&s->procUV)->advanced_block[block_y+1][block_x].ppp_x[TOP_RIGHT_CORNER])
                    (&s->procUV)->advanced_block[block_y+1][block_x].ppp_x[TOP_RIGHT_CORNER] = ( (&s->procUV)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER]+(&s->procUV)->advanced_block[block_y+1][block_x].ppp_x[TOP_RIGHT_CORNER])/2;
                if ((&s->procUV)->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER] < (&s->procUV)->advanced_block[block_y+1][block_x].ppp_y[TOP_LEFT_CORNER])
                    (&s->procUV)->advanced_block[block_y+1][block_x].ppp_y[TOP_LEFT_CORNER] =  ((&s->procUV)->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER]+(&s->procUV)->advanced_block[block_y+1][block_x].ppp_y[TOP_LEFT_CORNER] )/2;
                if ((&s->procUV)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER] < (&s->procUV)->advanced_block[block_y+1][block_x].ppp_y[TOP_RIGHT_CORNER])
                    (&s->procUV)->advanced_block[block_y+1][block_x].ppp_y[TOP_RIGHT_CORNER] = ( (&s->procUV)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER]+(&s->procUV)->advanced_block[block_y+1][block_x].ppp_y[TOP_RIGHT_CORNER])/2;
            }

            /*if (!(&s->procY)->advanced_block[block_y][block_x].empty_flagU) { 
                procUV->num_hopsU += num_hopsUV;
            }
            if (!(&s->procY)->advanced_block[block_y][block_x].empty_flagV) { 
                procUV->num_hopsV += num_hopsUV;
            }*/

        }
    }
}


//==================================================================
// BASIC LHE DECODING
//==================================================================
/**
 * Decodes one hop per pixel in a block
 * 
 * @param *prec Pointer to Lhe precalculated data
 * @param *proc LHE processing parameters
 * @param *lhe LHE image arrays
 * @param linesize rectangle images create a square image in ffmpeg memory. Linesize is width used by ffmpeg in memory
 * @param total_blocks_width number of blocks widthwise
 * @param block_x block x index
 * @param block_y block y index
 */


/**
 * Decodes one hop per pixel
 * 
 * @param *prec precalculated lhe data
 * @param *proc LHE processing parameters
 * @param *lhe LHE image arrays final result
 * @param linesize rectangle images create a square image in ffmpeg memory. Linesize is width used by ffmpeg in memory
 */
static void lhe_basic_decode_one_hop_per_pixel (LheBasicPrec *prec, LheProcessing *proc, LheImage *lhe, int linesize) {
       
    //Hops computation.
    bool small_hop, last_small_hop;
    uint8_t hop, predicted_luminance, hop_1; 
    int pix, pix_original_data, dif_pix, dato;
    int grad;

    //soft_counter = 0;
    //soft_threshold = 8;
    //soft_mode = false;
    grad = 0;
    
    small_hop           = false;
    last_small_hop      = true;        // indicates if last hop is small
    predicted_luminance = 0;            // predicted signal
    hop_1               = MIN_HOP_1;//START_HOP_1;
    pix                 = 0;            // pixel possition, from 0 to image size       
    pix_original_data   = 0;
    
    dif_pix = linesize - proc->width;

    for (int y=0; y < proc->height; y++)  {
        for (int x=0; x < proc->width; x++)     {
            
            hop = lhe->hops[pix_original_data]; 
            
            if (x==0 && y==0)
            {
                predicted_luminance=lhe->first_color_block[0];//first pixel always is perfectly predicted! :-)  
            }
            else if (y == 0)
            {
                predicted_luminance=lhe->component_prediction[pix-1];            
            }
            else if (x == 0)
            {
                predicted_luminance=lhe->component_prediction[pix-linesize];
                last_small_hop=true;
                hop_1=MIN_HOP_1;//START_HOP_1;
            } 
            else if (x == proc->width -1)
            {
                predicted_luminance=(lhe->component_prediction[pix-1]+lhe->component_prediction[pix-linesize])>>1;                                                       
            }
            else 
            {
                predicted_luminance=(lhe->component_prediction[pix-1]+lhe->component_prediction[pix+1-linesize])>>1;     
            }

            predicted_luminance = predicted_luminance + grad;
           
            if (hop != 4){
                if (hop == 5) grad = 1;
                else if (hop == 3) grad = -1;
                else grad = 0;
            }

            if (hop == 4){
                dato = predicted_luminance;
                small_hop = true;
            } else if (hop == 5) {
                dato = predicted_luminance + hop_1;
                small_hop = true;
            } else if (hop == 3) {
                dato = predicted_luminance - hop_1;
                small_hop = true;
            } else {
                small_hop = false;
                if (hop > 5) {
                    dato = 255 - prec->cache_hops[255-predicted_luminance][hop_1-4][8 - hop];
                } else {
                    dato = prec->cache_hops[predicted_luminance][hop_1-4][hop];
                }
            }

            //assignment of component_prediction
            lhe->component_prediction[pix]= dato;//prec -> prec_luminance[predicted_luminance][r_max][hop_1][hop];
            
            //tunning hop1 for the next hop ( "h1 adaptation")
            //------------------------------------------------
            if (hop>5 || hop<3) small_hop=false; //true by default
            if (small_hop==true && last_small_hop==true) {
                if (hop_1>MIN_HOP_1) hop_1--;
            } else {
                hop_1=MAX_HOP_1;
            }
            
            last_small_hop=small_hop;

            //lets go for the next pixel
            //--------------------------
            pix++;
            pix_original_data++;
        }// for x
        pix+=dif_pix;
    }// for y
}

/**
 * Calls methods to decode sequentially
 */
static void lhe_basic_decode_frame_sequential (LheState *s) 
{
    AVFrame *frame;
    LheBasicPrec *prec;
    LheProcessing *procY, *procUV;
    LheImage *lheY, *lheU, *lheV;
    
    frame = s->frame;
    prec = &s->prec;
    procY = &s->procY;
    procUV = &s->procUV;
    lheY = &s->lheY;
    lheU = &s->lheU;
    lheV = &s->lheV;
    
    //Luminance
    lhe_basic_decode_one_hop_per_pixel(prec, procY, lheY, frame->linesize[0]);

    //Chrominance U
    lhe_basic_decode_one_hop_per_pixel(prec, procUV, lheU, frame->linesize[1]);

    //Chrominance V
    lhe_basic_decode_one_hop_per_pixel(prec, procUV, lheV, frame->linesize[2]);
}

/**
 * Calls methods to decode pararell
 */

//==================================================================
// ADVANCED LHE DECODING
//==================================================================
/**
 * Decodes one hop per pixel in a block
 * 
 * @param *prec precalculated lhe data
 * @param *proc LHE processing parameters
 * @param *lhe LHE image arrays final result
 * @param total_blocks_width number of blocks widthwise
 * @param block_x block x index
 * @param block_y block y index
 */
static void lhe_advanced_decode_one_hop_per_pixel_block (LheBasicPrec *prec, LheProcessing *proc, LheImage *lhe,
                                                         uint32_t total_blocks_width, uint32_t block_x, uint32_t block_y) 
{
       
    //Hops computation.
    int xini, xfin_downsampled, yini, yfin_downsampled;
    bool small_hop, last_small_hop, soft_mode;
    int hop, predicted_luminance, hop_1; 
    int pix, dif_pix, dato, grad, ratioX, ratioY, y_prev, x_prev;
    
    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled; 
 
    yini = proc->basic_block[block_y][block_x].y_ini;
    yfin_downsampled = proc->advanced_block[block_y][block_x].y_fin_downsampled;
    
    small_hop           = false;
    last_small_hop      = true;        // indicates if last hop is small
    predicted_luminance = 0;            // predicted signal
    hop_1               = MIN_HOP_1;//START_HOP_1;
    pix                 = 0;            // pixel possition, from 0 to image size        
    grad = 0;

    pix = yini*proc->width + xini; 
    dif_pix = proc->width - xfin_downsampled + xini;

    ratioY = 1;
    if (block_x > 0){
        ratioY = 1000*(proc->advanced_block[block_y][block_x-1].y_fin_downsampled - proc->basic_block[block_y][block_x-1].y_ini)/(yfin_downsampled - yini);
    }

    ratioX = 1;
    if (block_y > 0){
        ratioX = 1000*(proc->advanced_block[block_y-1][block_x].x_fin_downsampled - proc->basic_block[block_y-1][block_x].x_ini)/(xfin_downsampled - xini);
    }

    for (int y=yini; y < yfin_downsampled; y++)  {
        y_prev = ((y-yini)*ratioY/1000)+yini;
        for (int x=xini; x < xfin_downsampled; x++)     {
            x_prev = ((x-xini)*ratioX/1000)+xini;
            
            hop = lhe->hops[pix];          
            if (y>yini && x>xini && x<(xfin_downsampled-1)) { //Interior del bloque
                predicted_luminance=(lhe->downsampled_image[pix-1]+lhe->downsampled_image[pix+1-proc->width])>>1;
            } else if (x==xini && y>yini) { //Lateral izquierdo
                if (x > 0) predicted_luminance=(lhe->downsampled_image[y_prev*proc->width+proc->advanced_block[block_y][block_x-1].x_fin_downsampled-1]+lhe->downsampled_image[pix-proc->width+1])/2;
                else predicted_luminance=lhe->downsampled_image[pix-proc->width];
                last_small_hop=true;
                hop_1=MIN_HOP_1;//START_HOP_1;
            } else if (y == yini) { //Lateral superior y pixel inicial
                if(x == 0 && y == 0) predicted_luminance=lhe->first_color_block[0];
                //Primer pixel de cualquier bloque de la fila superior de bloques
                else if (y == 0 && x == xini) predicted_luminance=lhe->downsampled_image[proc->advanced_block[block_y][block_x-1].x_fin_downsampled-1];
                //Cualquier pixel de la primera fila de bloques distinto al primer pixel.
                else if (y == 0) predicted_luminance=lhe->downsampled_image[pix-1];
                //Pixel inicial de la primera columna de bloques
                else if (x == 0) predicted_luminance=lhe->downsampled_image[(proc->advanced_block[block_y-1][block_x].y_fin_downsampled-1)*proc->width];
                //Pixel inicial de cualquier bloque interno
                else if (x == xini) predicted_luminance=(lhe->downsampled_image[yini*proc->width+proc->advanced_block[block_y][block_x-1].x_fin_downsampled-1]+lhe->downsampled_image[(proc->advanced_block[block_y-1][block_x].y_fin_downsampled-1)*proc->width+xini])/2;
                else predicted_luminance=(lhe->downsampled_image[pix-1]+lhe->downsampled_image[(proc->advanced_block[block_y-1][block_x].y_fin_downsampled-1)*proc->width+x_prev])/2;
            } else { //Lateral derecho
                predicted_luminance=(lhe->downsampled_image[pix-1]+lhe->downsampled_image[pix-proc->width])>>1;    
            }
            
            predicted_luminance = predicted_luminance + grad;
            if (predicted_luminance > 255) predicted_luminance = 255;
            else if (predicted_luminance < 1) predicted_luminance = 1;
            
            if (hop != 4){
                if (hop == 5) grad = 1;
                else if (hop == 3) grad = -1;
                else grad = 0;
            }

            if (hop == 4){
                dato = predicted_luminance;
                small_hop = true;
            } else if (hop == 5) {
                dato = predicted_luminance + hop_1;
                small_hop = true;
            } else if (hop == 3) {
                dato = predicted_luminance - hop_1;
                small_hop = true;
            } else {
                if (soft_mode) {
                    hop_1 = MIN_HOP_1;
                }
                small_hop = false;
                if (hop > 5) {
                    dato = 255 - prec->cache_hops[255-predicted_luminance][hop_1-4][8 - hop];
                } else {
                    dato = prec->cache_hops[predicted_luminance][hop_1-4][hop];
                }
            }

            //assignment of component_prediction
            //This is the uncompressed image
            lhe->downsampled_image[pix]=dato;//lhe->first_color_block[num_block];// dato;//prec -> prec_luminance[predicted_luminance][r_max][hop_1][hop];
            
            //tunning hop1 for the next hop ( "h1 adaptation")
            //------------------------------------------------
            if (hop>5 || hop<3) small_hop=false; //true by default
            
            if (small_hop==true && last_small_hop==true) {
                if (hop_1>MIN_HOP_1) hop_1--;
            } else {
                hop_1=MAX_HOP_1;
            }
            
            last_small_hop=small_hop;

            //lets go for the next pixel
            //--------------------------
            pix++;
        }// for x
        pix+=dif_pix;
    }// for y
}

/**
 * Vertical Nearest neighbour interpolation 
 * 
 * @param *proc LHE processing parameters
 * @param *lhe LHE image arrays
 * @param *intermediate_interpolated_image intermediate interpolated image 
 * @param block_x block x index
 * @param block_y block y index
 */
static void lhe_advanced_vertical_nearest_neighbour_interpolation (LheProcessing *proc, LheImage *lhe,
                                                                   uint8_t *intermediate_interpolated_image, 
                                                                   int block_x, int block_y) 
{
    uint32_t downsampled_y_side;
    float gradient, gradient_0, gradient_1, ppp_y, ppp_0, ppp_1, ppp_2, ppp_3;
    uint32_t xini, xfin_downsampled, yini, yprev_interpolated, yfin_interpolated, yfin_downsampled, downsampled_x_side;
    float yfin_interpolated_float;
    
    downsampled_y_side = proc->advanced_block[block_y][block_x].downsampled_y_side;
    downsampled_x_side = proc->advanced_block[block_y][block_x].downsampled_x_side;
    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled;
    yini = proc->basic_block[block_y][block_x].y_ini;
    yfin_downsampled = proc->advanced_block[block_y][block_x].y_fin_downsampled;

    ppp_0=proc->advanced_block[block_y][block_x].ppp_y[TOP_LEFT_CORNER];
    ppp_1=proc->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER];
    ppp_2=proc->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER];
    ppp_3=proc->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER];
    
    //gradient PPPy side c
    gradient_0=(ppp_1-ppp_0)/(downsampled_x_side-1.0);    
    //gradient PPPy side d
    gradient_1=(ppp_3-ppp_2)/(downsampled_x_side-1.0);
    
    // pppx initialized to ppp_0
    ppp_y=ppp_0;    
      
    for (int x=xini;x<xfin_downsampled;x++)
    {
            gradient=(ppp_2-ppp_0)/(downsampled_y_side-1.0);  
            
            ppp_y=ppp_0;

            //Interpolated y coordinates
            yprev_interpolated = yini; 
            yfin_interpolated_float= yini+ppp_y;

            // bucle for horizontal scanline 
            // scans the downsampled image, pixel by pixel
            for (int y_sc=yini;y_sc<yfin_downsampled;y_sc++)
            {            
                yfin_interpolated = yfin_interpolated_float + 0.5;  
                
                for (int i=yprev_interpolated;i < yfin_interpolated;i++)
                {
                    intermediate_interpolated_image[i*proc->width+x]=lhe->downsampled_image[y_sc*proc->width+x];                  
                }
          
                yprev_interpolated=yfin_interpolated;
                ppp_y+=gradient;
                yfin_interpolated_float+=ppp_y;               
                
            }//y
            ppp_0+=gradient_0;
            ppp_2+=gradient_1;
    }//x
    
}

/**
 * Horizontal Nearest neighbour interpolation 
 * 
 * @param *proc LHE processing parameters
 * @param *lhe LHE image arrays
 * @param *intermediate_interpolated_image intermediate interpolated image in y coordinate
 * @param linesize rectangle images create a square image in ffmpeg memory. Linesize is width used by ffmpeg in memory
 * @param block_x block x index
 * @param block_y block y index
 */
static void lhe_advanced_horizontal_nearest_neighbour_interpolation (LheProcessing *proc, LheImage *lhe,
                                                                     uint8_t *intermediate_interpolated_image, 
                                                                     int linesize, int block_x, int block_y) 
{
    uint32_t block_height, downsampled_x_side;
    float gradient, gradient_0, gradient_1, ppp_x, ppp_0, ppp_1, ppp_2, ppp_3;
    uint32_t xini, xfin_downsampled, xprev_interpolated, xfin_interpolated, yini, yfin;
    float xfin_interpolated_float;
    
    block_height = proc->basic_block[block_y][block_x].block_height;
    downsampled_x_side = proc->advanced_block[block_y][block_x].downsampled_x_side;
    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled;
    yini = proc->basic_block[block_y][block_x].y_ini;
    yfin =  proc->basic_block[block_y][block_x].y_fin;

    ppp_0=proc->advanced_block[block_y][block_x].ppp_x[TOP_LEFT_CORNER];
    ppp_1=proc->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER];
    ppp_2=proc->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER];
    ppp_3=proc->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER];
        
    //gradient PPPx side a
    gradient_0=(ppp_2-ppp_0)/(block_height-1.0);   
    //gradient PPPx side b
    gradient_1=(ppp_3-ppp_1)/(block_height-1.0);
    

    for (int y=yini; y<yfin; y++)
    {        
        gradient=(ppp_1-ppp_0)/(downsampled_x_side-1.0); 

        ppp_x=ppp_0;
        
        //Interpolated x coordinates
        xprev_interpolated = xini; 
        xfin_interpolated_float= xini+ppp_x;

        for (int x_sc=xini; x_sc<xfin_downsampled; x_sc++)
        {
            xfin_interpolated = xfin_interpolated_float + 0.5;            
               
            for (int i=xprev_interpolated;i < xfin_interpolated;i++)
            {
                lhe->component_prediction[y*linesize+i]=intermediate_interpolated_image[y*proc->width+x_sc];
                //PARA VER LINEAS VERDES EN LA FRONTERA DE LOS BLOQUES
                if (i == xini || y == yini) lhe->component_prediction[y*linesize+i]=0;
                //PARA SACAR LA IMAGEN DOWNSAMPLEADA
                //lhe->component_prediction[y*linesize+i]=lhe->downsampled_image[y*proc->width+i];
                //PARA SACAR LA IMAGEN DELTA
                //lhe->component_prediction[y*linesize+i]=delta_prediction_Y_dec[y*proc->width+i];
            }
                        
            xprev_interpolated=xfin_interpolated;
            ppp_x+=gradient;
            xfin_interpolated_float+=ppp_x;   
        }//x

        ppp_0+=gradient_0;
        ppp_1+=gradient_1;

    }//y 

    /*for (int y=yini; y<yfin; y++)
    {   
        for (int x=xini; x<xfin; x++)
        {             
            delta_prediction_Y_dec[y*proc->width+x]=0;
        }
    }*/
}

/** 
 * Vertical Adaptative neighbour interpolation 
 * 
 * This interpolation mixes Bilinear and Nearest Neighbour Interpolation. It 
 * chooses betwen those using the perceptual relevande of the image. This
 *  vertical interpolation must be run before the horizontal.
 *  
 * @param *proc LHE processing parameters 
 * @param *lhe LHE image arrays 
 * @param *intermediate_interpolated_image intermediate interpolated image  
 * @param block_x block x index 
 * @param block_y block y index 
 */
static void lhe_advanced_vertical_adaptative_interpolation(LheProcessing *proc, 
                                            LheImage *lhe, uint8_t *intermediate_interpolated_image,
                                            int block_x, int block_y, int vertical_blocks, bool only_bilineal)
{
    uint32_t downsampled_y_side, downsampled_x_side;
    float gradient, gradient_0, gradient_1, gradient_pr, gradient_0_pr, 
        gradient_1_pr, ppp_y, ppp_0, ppp_1, ppp_2, ppp_3, pr_y, pr_0, pr_1,
        pr_2, pr_3, yfin_interpolated_float, x_side_relation,
        past_x_side_relation;
    uint32_t xini, xfin_downsampled, yfin, yini, yprev_interpolated, 
        yfin_interpolated, yfin_downsampled, half;
    uint8_t next_block_downsampled_x_side, past_block_downsampled_x_side;
    bool last_block, first_block;

    downsampled_y_side = proc->advanced_block[block_y][block_x].downsampled_y_side;
    downsampled_x_side = proc->advanced_block[block_y][block_x].downsampled_x_side;
    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled;
    yini = proc->basic_block[block_y][block_x].y_ini;
    yfin_downsampled = proc->advanced_block[block_y][block_x].y_fin_downsampled;
    yfin = proc->basic_block[block_y][block_x].y_fin;

    ppp_0 = proc->advanced_block[block_y][block_x].ppp_y[TOP_LEFT_CORNER];
    ppp_1 = proc->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER];
    ppp_2 = proc->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER];
    ppp_3 = proc->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER];

    pr_0 = proc->perceptual_relevance_y[block_y][block_x];
    pr_1 = proc->perceptual_relevance_y[block_y][block_x + 1];
    pr_2 = proc->perceptual_relevance_y[block_y + 1][block_x];
    pr_3 = proc->perceptual_relevance_y[block_y + 1][block_x + 1];
    if (block_y == vertical_blocks - 1)
    {
        last_block = true;
    }
    else
    {
        last_block = false;
        next_block_downsampled_x_side = proc->advanced_block[block_y + 1][block_x].downsampled_x_side;
        x_side_relation = (next_block_downsampled_x_side - 1) / (float)(downsampled_x_side - 1);
    }
    if(block_y == 0){
        first_block = true;
    }
    else
    {
        first_block = false;
        past_block_downsampled_x_side = proc->advanced_block[block_y-1][block_x].downsampled_x_side;
        past_x_side_relation = (past_block_downsampled_x_side-1)/(float)(downsampled_x_side-1);
    }

    //gradient PPPy side c
    gradient_0 = (ppp_1 - ppp_0) / (downsampled_x_side - 1.0);
    gradient_0_pr = (pr_1 - pr_0) / (downsampled_x_side - 1.0);
    //gradient PPPy side d
    gradient_1 = (ppp_3 - ppp_2) / (downsampled_x_side - 1.0);
    gradient_1_pr = (pr_3 - pr_2) / (downsampled_x_side - 1.0);

    for (int x = xini; x < xfin_downsampled; x++)
    {
        gradient = (ppp_2 - ppp_0) / (downsampled_y_side - 1.0);
        gradient_pr = (pr_2 - pr_0) / (downsampled_y_side - 1.0);
        ppp_y = ppp_0;
        pr_y = pr_0;

        //Interpolated y coordinates
        yprev_interpolated = yini;
        yfin_interpolated_float = yini + ppp_y;

        // bucle for horizontal scanline
        // scans the downsampled image, pixel by pixel
        for (int y_sc = yini; y_sc < yfin_downsampled; y_sc++)
        {
            yfin_interpolated = yfin_interpolated_float + 0.5;

            for (int i = yprev_interpolated; i < yfin_interpolated; i++)
            {
                if (pr_y < 0.251 || only_bilineal) // Bilinear
                {
                    half = yprev_interpolated + ((yfin_interpolated - yprev_interpolated) / 2); // This threshold decided whether look next pix or previous one.
                    if (i >= half && y_sc != yfin_downsampled - 1)
                    {
                        intermediate_interpolated_image[i * proc->width + x] = (lhe->downsampled_image[y_sc * proc->width + x] * (yfin_interpolated - yprev_interpolated - i + half) + lhe->downsampled_image[(y_sc + 1) * proc->width + x] * (i - half)) / (yfin_interpolated - yprev_interpolated);
                    }
                    else if (i < half && y_sc != yini)
                    {
                        intermediate_interpolated_image[i * proc->width + x] = (lhe->downsampled_image[y_sc * proc->width + x] * (yfin_interpolated - yprev_interpolated + i - half) + lhe->downsampled_image[(y_sc - 1) * proc->width + x] * (half - i)) / (yfin_interpolated - yprev_interpolated);
                    }
                    else if (i >= half && y_sc == yfin_downsampled - 1 && !last_block) //Bilinear neighbouring with the next block for the last scanlines
                    {
                        float next_value_x = xini + (x - xini) * x_side_relation;
                        float next_value_index = yfin * proc->width + next_value_x;
                        int contribution = (int)((next_value_x - ((int)next_value_x)) * 100);
                        int value = (lhe->downsampled_image[(int)next_value_index] * (100 - contribution) + lhe->downsampled_image[(int)next_value_index + 1] * contribution) / 100;
                        int value_y = yfin + ((yfin_interpolated - yprev_interpolated) / 2); // This is an aproximation. I estimate the next ppy/2 as the same in this block.
                        intermediate_interpolated_image[i * proc->width + x] = (lhe->downsampled_image[y_sc * proc->width + x] * (value_y - i) + value * (i - half)) / (value_y - half);
                    }
                    else if (i < half && y_sc == yini && !first_block) //Bilinear neighbouring with the past block for the first scanlines
                    {
                        float past_value_x = xini + (x - xini) * past_x_side_relation;
                        float past_value_index = (yini - 1) * proc->width + past_value_x;
                        int contribution = (int)((past_value_x - ((int)past_value_x)) * 100);
                        int value = (intermediate_interpolated_image[(int)past_value_index] * (100 - contribution) + intermediate_interpolated_image[(int)past_value_index + 1] * contribution) / 100;
                        intermediate_interpolated_image[i * proc->width + x] = (lhe->downsampled_image[y_sc * proc->width + x] * (i - (yini - 1)) + value * (half - i)) / (half - (yini - 1));
                    }
                    else // Nearest neighbour for the rest of cases
                    {
                        intermediate_interpolated_image[i * proc->width + x] = lhe->downsampled_image[y_sc * proc->width + x];
                    }
                }
                else // Nearest neighbour for the rest of cases
                {
                    intermediate_interpolated_image[i * proc->width + x] = lhe->downsampled_image[y_sc * proc->width + x];
                }
            }
            yprev_interpolated = yfin_interpolated;
            ppp_y += gradient;
            pr_y += gradient_pr;
            yfin_interpolated_float += ppp_y;

        } //y
        ppp_0 += gradient_0;
        ppp_2 += gradient_1;
        pr_0 += gradient_0_pr;
        pr_2 += gradient_1_pr;
    } //x
}

/**
 * Horizontal Adaptative Interpolation 
 * 
 *  This interpolation mixes Bilinear and Nearest Neighbour Interpolation. It 
 * chooses betwen those using the perceptual relevande of the image. This
 *  vertical interpolation must be run after the vertical.
 * 
 * @param *proc LHE processing parameters
 * @param *lhe LHE image arrays
 * @param *intermediate_interpolated_image intermediate interpolated image in y coordinate
 * @param linesize rectangle images create a square image in ffmpeg memory. Linesize is width used by ffmpeg in memory
 * @param block_x block x index
 * @param block_y block y index
 */
static void lhe_advanced_horizontal_adaptative_interpolation(LheProcessing *proc, LheImage *lhe,
                                                         uint8_t *intermediate_interpolated_image,
                                                         int linesize, int block_x, int block_y, bool only_bilineal)
{
    uint32_t block_height, downsampled_x_side, half;
    float gradient, gradient_0, gradient_1, gradient_pr, gradient_0_pr, gradient_1_pr,
        ppp_x, ppp_0, ppp_1, ppp_2, ppp_3, pr_x, pr_0, pr_1, pr_2, pr_3;
    uint32_t xini, xfin, xfin_downsampled, xprev_interpolated, xfin_interpolated,
        yini, yfin;
    float xfin_interpolated_float;
    bool last_block, first_block;
    block_height = proc->basic_block[block_y][block_x].block_height;
    downsampled_x_side = proc->advanced_block[block_y][block_x].downsampled_x_side;
    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin = proc->basic_block[block_y][block_x].x_fin;
    xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled;
    yini = proc->basic_block[block_y][block_x].y_ini;
    yfin = proc->basic_block[block_y][block_x].y_fin;

    ppp_0 = proc->advanced_block[block_y][block_x].ppp_x[TOP_LEFT_CORNER];
    ppp_1 = proc->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER];
    ppp_2 = proc->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER];
    ppp_3 = proc->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER];

    pr_0 = proc->perceptual_relevance_x[block_y][block_x];
    pr_1 = proc->perceptual_relevance_x[block_y][block_x + 1];
    pr_2 = proc->perceptual_relevance_x[block_y + 1][block_x];
    pr_3 = proc->perceptual_relevance_x[block_y + 1][block_x + 1];

    if (block_x == HORIZONTAL_BLOCKS - 1)
    {
        last_block = true;
    }
    else
    {
        last_block = false;
    }
    if (block_x == 0)
    {
        first_block = true;
    }
    else
    {
        first_block = false;
    }

    //gradient PPPx side a
    gradient_0 = (ppp_2 - ppp_0) / (block_height - 1.0);
    gradient_0_pr = (pr_2 - pr_0) / (block_height - 1.0);
    //gradient PPPx side b
    gradient_1 = (ppp_3 - ppp_1) / (block_height - 1.0);
    gradient_1_pr = (pr_3 - pr_1) / (block_height - 1.0);

    for (int y = yini; y < yfin; y++)
    {
        gradient = (ppp_1 - ppp_0) / (downsampled_x_side - 1.0);
        gradient_pr = (pr_1 - pr_0) / (downsampled_x_side - 1.0);
        ppp_x = ppp_0;
        pr_x = pr_0;
        //Interpolated x coordinates
        xprev_interpolated = xini;
        xfin_interpolated_float = xini + ppp_x;

        for (int x_sc = xini; x_sc < xfin_downsampled; x_sc++)
        {
            xfin_interpolated = xfin_interpolated_float + 0.5;

            for (int i = xprev_interpolated; i < xfin_interpolated; i++)
            {
                if (pr_x < 0.251 || only_bilineal) // Bilinear
                {
                    half = xprev_interpolated + ((xfin_interpolated - xprev_interpolated) / 2);
                    if (i >= half && x_sc != xfin_downsampled - 1)
                    {
                        lhe->component_prediction[y * linesize + i] = (intermediate_interpolated_image[y * proc->width + x_sc] * (xfin_interpolated - xprev_interpolated - i + half) + intermediate_interpolated_image[y * proc->width + x_sc + 1] * (i - half)) / (xfin_interpolated - xprev_interpolated);
                    }
                    else if (i < half && x_sc != xini)
                    {
                        lhe->component_prediction[y * linesize + i] = (intermediate_interpolated_image[y * proc->width + x_sc] * (xfin_interpolated - xprev_interpolated + i - half) + intermediate_interpolated_image[y * proc->width + x_sc - 1] * (half - i)) / (xfin_interpolated - xprev_interpolated);
                    }
                    else if (i >= half && x_sc == xfin_downsampled - 1 && !last_block) //Must look into the next block
                    {
                        int next_value_index = y * proc->width + xfin;
                        int value_x = xfin + ((xfin_interpolated - xprev_interpolated) / 2); // This is an aproximation. I estimate the next ppy/2 as the same in this block.
                        lhe->component_prediction[y * linesize + i] = (intermediate_interpolated_image[y * proc->width + x_sc] * (value_x - i) + intermediate_interpolated_image[next_value_index] * (i - half)) / (value_x - half);
                    }
                    else if (i < half && x_sc == xini && !first_block) //Must look into the past block
                    {
                        int past_value_index = y * linesize + (xini - 1);
                        lhe->component_prediction[y * linesize + i] = (intermediate_interpolated_image[y * proc->width + x_sc] * (i - (xini - 1)) + lhe->component_prediction[past_value_index] * (half - i)) / (half - (xini - 1));
                    }
                    else
                    {
                        lhe->component_prediction[y * linesize + i] = intermediate_interpolated_image[y * proc->width + x_sc];
                    }
                }
                else // Nearest neighbour
                {
                    lhe->component_prediction[y * linesize + i] = intermediate_interpolated_image[y * proc->width + x_sc];
                }
            }

            xprev_interpolated = xfin_interpolated;
            ppp_x += gradient;
            pr_x += gradient_pr;
            xfin_interpolated_float += ppp_x;
        } //x
        ppp_0 += gradient_0;
        ppp_1 += gradient_1;
        pr_0 += gradient_0_pr;
        pr_1 += gradient_1_pr;
    } //y
}


/**
 * Decodes symbols in advanced LHE file
 * 
 * @param *s LHE State
 * @param *he_Y LHE Huffman Entry for luminance
 * @param *he_UV LHE Huffman Entry for chrominances
 * @param image_size_Y luminance image size
 * @param image_size_UV chrominance image size
 */
static void lhe_advanced_decode_symbols(LheState *s, uint32_t image_size_Y, uint32_t image_size_UV)
{
    // Copy the pr fron Y to UV
    s->procUV.perceptual_relevance_y = s->procY.perceptual_relevance_y;
    s->procUV.perceptual_relevance_x = s->procY.perceptual_relevance_x;
    //#pragma omp parallel for
    for (int i = -(int)s->total_blocks_height+1; i < (int)s->total_blocks_width; i++){
        #pragma omp parallel for
        for (int block_y=s->total_blocks_height-1; block_y>=0; block_y--) 
        {
            int block_x = i + s->total_blocks_height -1 - block_y;
            if (block_x >= 0 && block_x < s->total_blocks_width) {
                //Luminance
                lhe_advanced_decode_one_hop_per_pixel_block(&s->prec, &s->procY, &s->lheY, s->total_blocks_width, block_x, block_y);
                //Chrominance U
                lhe_advanced_decode_one_hop_per_pixel_block(&s->prec, &s->procUV, &s->lheU, s->total_blocks_width, block_x, block_y);
                //Chrominance V
                lhe_advanced_decode_one_hop_per_pixel_block(&s->prec, &s->procUV, &s->lheV, s->total_blocks_width, block_x, block_y);
            }
        }

    }

    for (int i = -(int)s->total_blocks_height+1; i < (int)s->total_blocks_width; i++){
        #pragma omp parallel for
        for (int block_y=s->total_blocks_height-1; block_y>=0; block_y--) 
        {
            int block_x = i + s->total_blocks_height -1 - block_y;
            if (block_x >= 0 && block_x < s->total_blocks_width) {
                //Luminance

                lhe_advanced_vertical_adaptative_interpolation(&s->procY, &s->lheY, intermediate_interpolated_Y,
                                                                      block_x, block_y, s->total_blocks_height, false);
                //Chrominance U
                lhe_advanced_vertical_adaptative_interpolation(&s->procUV, &s->lheU, intermediate_interpolated_U,
                                                                      block_x, block_y, s->total_blocks_height,true);
                //Chrominance V
                lhe_advanced_vertical_adaptative_interpolation(&s->procUV, &s->lheV, intermediate_interpolated_V,
                                                                      block_x, block_y, s->total_blocks_height,true);
            }
        }
    }

    for (int i = -(int)s->total_blocks_height+1; i < (int)s->total_blocks_width; i++){
        #pragma omp parallel for
        for (int block_y=s->total_blocks_height-1; block_y>=0; block_y--) 
        {
            int block_x = i + s->total_blocks_height -1 - block_y;
            if (block_x >= 0 && block_x < s->total_blocks_width) {
                //Luminance

                lhe_advanced_horizontal_adaptative_interpolation(&s->procY, &s->lheY, intermediate_interpolated_Y,
                                                                        s->frame->linesize[0], block_x, block_y,false);
                //Chrominance U
                lhe_advanced_horizontal_adaptative_interpolation(&s->procUV, &s->lheU, intermediate_interpolated_U,
                                                                        s->frame->linesize[1], block_x, block_y,true);
                //Chrominance V
                lhe_advanced_horizontal_adaptative_interpolation(&s->procUV, &s->lheV, intermediate_interpolated_V,
                                                                        s->frame->linesize[2], block_x, block_y,true);
            }
        }
    }
}

//==================================================================
// VIDEO LHE DECODING
//==================================================================
/**
 * Decodes one hop per pixel in a block
 * 
 * @param *prec Pointer to precalculated lhe data
 * @param *proc LHE processing parameters
 * @param *lhe LHE image arrays
 * @param *delta_prediction Quantized differential information
 * @param total_blocks_width number of blocks widthwise
 * @param block_x block x index
 * @param block_y block y index
 */
static void mlhe_decode_delta (LheBasicPrec *prec, LheProcessing *proc, LheImage *lhe,
                               uint8_t *delta_prediction, uint8_t *adapted_downsampled_image,
                               uint32_t total_blocks_width, uint32_t block_x, uint32_t block_y) 
{
       
    //Hops computation.
    int xini, xfin_downsampled, yini, yfin_downsampled;
    bool small_hop, last_small_hop;
    int hop, predicted_luminance, hop_1; 
    int pix, dif_pix;
    int delta, image, ratioY, ratioX, y_prev;
/////////////////////////////////////////
    int tramo1, tramo2, signo;

    tramo1 = 52;
    tramo2 = 204;
////////////////////////////////////////////////////
    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled; 
 
    yini = proc->basic_block[block_y][block_x].y_ini;
    yfin_downsampled = proc->advanced_block[block_y][block_x].y_fin_downsampled;
    
    small_hop           = false;
    last_small_hop      = true;        // indicates if last hop is small
    predicted_luminance = 0;            // predicted signal
    hop_1               = MIN_HOP_1;//START_HOP_1;
    pix                 = 0;            // pixel possition, from 0 to image size
 
    pix = yini*proc->width + xini; 
    dif_pix = proc->width - xfin_downsampled + xini;
    
    ratioY = 1;
    if (block_x > 0){
        ratioY = 1000*(proc->advanced_block[block_y][block_x-1].y_fin_downsampled - proc->basic_block[block_y][block_x-1].y_ini)/(yfin_downsampled - yini);
    }

    ratioX = 1;
    if (block_y > 0){
        ratioX = 1000*(proc->advanced_block[block_y-1][block_x].x_fin_downsampled - proc->basic_block[block_y-1][block_x].x_ini)/(xfin_downsampled - xini);
    }

    for (int y=yini; y < yfin_downsampled; y++)  {
        y_prev = ((y-yini)*ratioY/1000)+yini;
        for (int x=xini; x < xfin_downsampled; x++)     {
            int x_prev = ((x-xini)*ratioX/1000)+xini;
            hop = lhe->hops[pix];          
  
            if (y>yini && x>xini && x<(xfin_downsampled-1)) { //Interior del bloque
                predicted_luminance=(delta_prediction[pix-1]+delta_prediction[pix+1-proc->width])>>1;
            } else if (x==xini && y>yini) { //Lateral izquierdo
                if (x > 0) predicted_luminance=(delta_prediction[y_prev*proc->width+proc->advanced_block[block_y][block_x-1].x_fin_downsampled-1]+delta_prediction[pix-proc->width+1])/2;
                else predicted_luminance=delta_prediction[pix-proc->width];
                last_small_hop=true;
                hop_1=MIN_HOP_1;//START_HOP_1;
            } else if (y == yini) { //Lateral superior y pixel inicial
                if(x == 0 && y == 0) predicted_luminance=lhe->first_color_block[0];
                //Primer pixel de cualquier bloque de la fila superior de bloques
                else if (y == 0 && x == xini) predicted_luminance=delta_prediction[proc->advanced_block[block_y][block_x-1].x_fin_downsampled-1];
                //Cualquier pixel de la primera fila de bloques distinto al primer pixel.
                else if (y == 0) predicted_luminance=delta_prediction[pix-1];
                //Pixel inicial de la primera columna de bloques
                else if (x == 0) predicted_luminance=delta_prediction[(proc->advanced_block[block_y-1][block_x].y_fin_downsampled-1)*proc->width];
                //Pixel inicial de cualquier bloque interno
                else if (x == xini) predicted_luminance=(delta_prediction[yini*proc->width+proc->advanced_block[block_y][block_x-1].x_fin_downsampled-1]+delta_prediction[(proc->advanced_block[block_y-1][block_x].y_fin_downsampled-1)*proc->width+xini])/2;
                else predicted_luminance=(delta_prediction[pix-1]+delta_prediction[(proc->advanced_block[block_y-1][block_x].y_fin_downsampled-1)*proc->width+x_prev])/2;
            } else { //Lateral derecho
                predicted_luminance=(delta_prediction[pix-1]+delta_prediction[pix-proc->width])>>1;    
            }

            if (hop == 4){
                delta = predicted_luminance;
                small_hop = true;
            } else if (hop == 5) {
                delta = predicted_luminance + hop_1;
                small_hop = true;
            } else if (hop == 3) {
                delta = predicted_luminance - hop_1;
                small_hop = true;
            } else {
                small_hop = false;
                if (hop > 5) {
                    delta = 255 - prec->cache_hops[255-predicted_luminance][hop_1-4][8 - hop];
                } else {
                    delta = prec->cache_hops[predicted_luminance][hop_1-4][hop];
                }
            }
            
            delta_prediction[pix] = delta;
        
            //tunning hop1 for the next hop ( "h1 adaptation")
            //------------------------------------------------
            if (hop>5 || hop<3) small_hop=false; //true by default
            if (small_hop==true && last_small_hop==true) {
                if (hop_1>MIN_HOP_1) hop_1--;
            } else {
                hop_1=MAX_HOP_1;
            }
            
            last_small_hop=small_hop;


            delta = delta-128;
            signo = 0;
            if (delta < 0) {
                signo = 1;
                delta = -delta;
            }

            if (delta < tramo1){
                if (signo == 0) image = adapted_downsampled_image[pix] + delta;
                else image = adapted_downsampled_image[pix] - delta;
             } else  if (delta <= tramo1+(tramo2-tramo1)/2){
                delta = (delta - tramo1)*2;
                delta += tramo1;
                if (signo == 0) image = adapted_downsampled_image[pix] + delta;
                else image = adapted_downsampled_image[pix] - delta;
            } else {
                delta = (delta - (tramo2 - tramo1)/2 - tramo1)*4;
                delta += tramo2;
                if (signo == 0) image = adapted_downsampled_image[pix] + delta;
                else image = adapted_downsampled_image[pix] - delta;
            }
            
            if (image > 255) 
            {
                image = 255;
            }
            else if (image < 1) 
            {
                image = 1;
            }
            
            lhe->downsampled_image[pix] = image;

            //lets go for the next pixel
            //--------------------------
            pix++;
        }// for x
        pix+=dif_pix;
    }// for y
}

/**
 * Decodes differential frame
 * 
 * @param *s LHE Context
 * @param *he_Y luminance Huffman data
 * @param *he_UV chrominance Huffman data
 * @param image_size_Y luminance image size
 * @param image_size_UV chrominance image size
 */
static void mlhe_decode_delta_frame (LheState *s, uint32_t image_size_Y, uint32_t image_size_UV) 
{
       
    //#pragma omp parallel for
    for (int block_y=0; block_y<s->total_blocks_height; block_y++)
    {
        for (int block_x=0; block_x<s->total_blocks_width; block_x++)
        {                             
            //Luminance
            mlhe_adapt_downsampled_data_resolution2 (&s->procY, &s->lheY,
                                                    intermediate_adapted_downsampled_data_Y_dec, adapted_downsampled_image_Y,
                                                    block_x, block_y);
            
            mlhe_decode_delta (&s->prec, &s->procY, &s->lheY, delta_prediction_Y_dec, 
                               adapted_downsampled_image_Y, s->total_blocks_width, block_x, block_y);

            
            lhe_advanced_vertical_nearest_neighbour_interpolation (&s->procY, &s->lheY, intermediate_interpolated_Y, 
                                                                   block_x, block_y);
                      
            lhe_advanced_horizontal_nearest_neighbour_interpolation (&s->procY, &s->lheY, intermediate_interpolated_Y, 
                                                                     s->frame->linesize[0], block_x, block_y);

            //Chrominance U                    
            mlhe_adapt_downsampled_data_resolution2 (&s->procUV, &s->lheU,
                                                    intermediate_adapted_downsampled_data_U_dec, adapted_downsampled_image_U,
                                                    block_x, block_y);
            
            mlhe_decode_delta (&s->prec, &s->procUV, &s->lheU, delta_prediction_U_dec, 
                               adapted_downsampled_image_U, s->total_blocks_width, block_x, block_y);
            
            lhe_advanced_vertical_nearest_neighbour_interpolation (&s->procUV, &s->lheU, intermediate_interpolated_U, 
                                                                   block_x, block_y);         
            
            lhe_advanced_horizontal_nearest_neighbour_interpolation (&s->procUV, &s->lheU, intermediate_interpolated_U, 
                                                                     s->frame->linesize[1], block_x, block_y);
            //Chrominance V            
            mlhe_adapt_downsampled_data_resolution2 (&s->procUV, &s->lheV, 
                                                    intermediate_adapted_downsampled_data_V_dec, adapted_downsampled_image_V,
                                                    block_x, block_y);
            
            mlhe_decode_delta (&s->prec, &s->procUV, &s->lheV, delta_prediction_V_dec, 
                               adapted_downsampled_image_V, s->total_blocks_width, block_x, block_y);
             
            lhe_advanced_vertical_nearest_neighbour_interpolation (&s->procUV, &s->lheV, intermediate_interpolated_V, 
                                                                   block_x, block_y);
            
            
            lhe_advanced_horizontal_nearest_neighbour_interpolation (&s->procUV, &s->lheV, intermediate_interpolated_V, 
                                                                     s->frame->linesize[2], block_x, block_y);    
        }
    }     
}

//==================================================================
// DECODE FRAME
//==================================================================
/**
 * Read and decodes LHE image
 *
 * @param *avctx Codec context
 * @param *data data from file
 * @param *got_frame indicates frame is ready
 * @param *avpkt AV packet
 */ 
static int lhe_decode_frame(AVCodecContext *avctx, void *data, int *got_frame, AVPacket *avpkt)
{    

    uint32_t pixels_block, image_size_Y, image_size_UV, ppp_max_theoric;
    int ret;
    float compression_factor;
    
    LheHuffEntry he_mesh[LHE_MAX_HUFF_SIZE_MESH];
    LheState *s = avctx->priv_data;
    
    const uint8_t *lhe_data = avpkt->data;
    
    init_get_bits(&s->gb, lhe_data, avpkt->size * 8);
    
    //LHE mode
    s->lhe_mode = get_bits(&s->gb, LHE_MODE_SIZE_BITS);
    
    image_size_Y = (&s->procY)->width * (&s->procY)->height;
    image_size_UV = (&s->procUV)->width * (&s->procUV)->height;
    //Allocates frame
    av_frame_unref(s->frame);
    if ((ret = ff_get_buffer(avctx, s->frame, 0)) < 0)
        return ret;
 
    if (s->lhe_mode == BASIC_LHE) 
    {
        s->total_blocks_width = 1;
        s->total_blocks_height = 1;
        goto basic;
    }
    
    //Pointers to different color components
    (&s->lheY)->component_prediction = s->frame->data[0];
    (&s->lheU)->component_prediction  = s->frame->data[1];
    (&s->lheV)->component_prediction  = s->frame->data[2];
    
    if (s->lhe_mode == ADVANCED_LHE) /*ADVANCED LHE*/
    {

        s->pixel_format = get_bits(&s->gb, PIXEL_FMT_SIZE_BITS);
        (&s->procY)->width = get_bits(&s->gb, 16);
        avctx->width = (&s->procY)->width;
        (&s->procY)->height = get_bits(&s->gb, 16);
        avctx->height = (&s->procY)->height;

        if (s->pixel_format == LHE_YUV420)
        {
            av_log(NULL, AV_LOG_INFO, "Pix fmt 420 del avanzado\n");
            s->chroma_factor_width = 2;
            s->chroma_factor_height = 2;
            avctx->pix_fmt = AV_PIX_FMT_YUV420P;
        } else if (s->pixel_format == LHE_YUV422) 
        {
            av_log(NULL, AV_LOG_INFO, "Pix fmt 422\n");
            s->chroma_factor_width = 2;
            s->chroma_factor_height = 1;
            avctx->pix_fmt = AV_PIX_FMT_YUV422P;
        } else if (s->pixel_format == LHE_YUV444) 
        {
            av_log(NULL, AV_LOG_INFO, "Pix fmt 444\n");
            s->chroma_factor_width = 1;
            s->chroma_factor_height = 1;
            avctx->pix_fmt = AV_PIX_FMT_YUV444P;
        }

        image_size_Y = (&s->procY)->width * (&s->procY)->height;
        (&s->procUV)->width = ((&s->procY)->width - 1)/s->chroma_factor_width + 1;
        (&s->procUV)->height = ((&s->procY)->height - 1)/s->chroma_factor_height + 1;
        image_size_UV = (&s->procUV)->width * (&s->procUV)->height;

        pixels_block = (&s->procY)->width / HORIZONTAL_BLOCKS;
        s->total_blocks_height = (&s->procY)->height / pixels_block;

        (&s->procY)-> theoretical_block_width = (&s->procY)->width / s->total_blocks_width;    
        (&s->procY)-> theoretical_block_height = (&s->procY)->height / s->total_blocks_height;   
        
        (&s->procUV)-> theoretical_block_width = (&s->procUV)->width / s->total_blocks_width;
        (&s->procUV)-> theoretical_block_height = (&s->procUV)->height / s->total_blocks_height; 

        //Realloc of the buffers of the image
        lhedec_free_tables(s);
        lhedec_alloc_tables(avctx, s);

        //Allocates frame
        av_frame_unref(s->frame);
        if ((ret = ff_get_buffer(avctx, s->frame, 0)) < 0)
            return ret;

        //First pixel array 
        (&s->lheY)->first_color_block[0] = get_bits(&s->gb, FIRST_COLOR_SIZE_BITS);
        (&s->lheU)->first_color_block[0] = get_bits(&s->gb, FIRST_COLOR_SIZE_BITS);
        (&s->lheV)->first_color_block[0] = get_bits(&s->gb, FIRST_COLOR_SIZE_BITS);

        (&s->lheY)->component_prediction = s->frame->data[0];
        (&s->lheU)->component_prediction  = s->frame->data[1];
        (&s->lheV)->component_prediction  = s->frame->data[2];

        //MESH Huffman
        lhe_read_huffman_table(s, he_mesh, LHE_MAX_HUFF_SIZE_MESH, LHE_HUFFMAN_NODE_BITS_MESH, LHE_HUFFMAN_NO_OCCURRENCES_MESH);

        //Read quality level and calculate compression factor
        s->quality_level = get_bits(&s->gb, QL_SIZE_BITS); 
        ppp_max_theoric = (&s->procY)-> theoretical_block_width/SIDE_MIN;
        if (ppp_max_theoric > PPP_MAX) ppp_max_theoric = PPP_MAX;
        compression_factor = (&s->prec)->compression_factor[ppp_max_theoric][s->quality_level]; 

        lhe_advanced_read_mesh(s, he_mesh, ppp_max_theoric, compression_factor);

        lhe_advanced_read_all_file_symbols (s);
              
        lhe_advanced_decode_symbols (s, image_size_Y, image_size_UV);     
        
    }

    else /*BASIC LHE*/       
    {

        
        basic:
        
        s->pixel_format = get_bits(&s->gb, PIXEL_FMT_SIZE_BITS);
        (&s->procY)->width = get_bits(&s->gb, 16);
        avctx->width = (&s->procY)->width;
        (&s->procY)->height = get_bits(&s->gb, 16);
        avctx->height = (&s->procY)->height;

        if (s->pixel_format == LHE_YUV420)
        {
            av_log(NULL, AV_LOG_INFO, "Pix fmt 420 del basico\n");
            s->chroma_factor_width = 2;
            s->chroma_factor_height = 2;
            avctx->pix_fmt = AV_PIX_FMT_YUV420P;
        } else if (s->pixel_format == LHE_YUV422) 
        {
            av_log(NULL, AV_LOG_INFO, "Pix fmt 422\n");
            s->chroma_factor_width = 2;
            s->chroma_factor_height = 1;
            avctx->pix_fmt = AV_PIX_FMT_YUV422P;
        } else if (s->pixel_format == LHE_YUV444) 
        {
            av_log(NULL, AV_LOG_INFO, "Pix fmt 444\n");
            s->chroma_factor_width = 1;
            s->chroma_factor_height = 1;
            avctx->pix_fmt = AV_PIX_FMT_YUV444P;
        }

        image_size_Y = (&s->procY)->width * (&s->procY)->height;
        (&s->procUV)->width = ((&s->procY)->width - 1)/s->chroma_factor_width + 1;
        (&s->procUV)->height = ((&s->procY)->height - 1)/s->chroma_factor_height + 1;
        image_size_UV = (&s->procUV)->width * (&s->procUV)->height;

        //Realloc of the buffers of the image
        lhedec_free_tables(s);
        lhedec_alloc_tables(avctx, s);
        
        //Allocates frame
        av_frame_unref(s->frame);
        if ((ret = ff_get_buffer(avctx, s->frame, 0)) < 0)
            return ret;

        //First pixel array 
        (&s->lheY)->first_color_block[0] = get_bits(&s->gb, FIRST_COLOR_SIZE_BITS);
        (&s->lheU)->first_color_block[0] = get_bits(&s->gb, FIRST_COLOR_SIZE_BITS);
        (&s->lheV)->first_color_block[0] = get_bits(&s->gb, FIRST_COLOR_SIZE_BITS);


        //Pointers to different color components
        (&s->lheY)->component_prediction = s->frame->data[0];
        (&s->lheU)->component_prediction  = s->frame->data[1];
        (&s->lheV)->component_prediction  = s->frame->data[2];

        (&s->procY)->num_hopsY = image_size_Y;
        (&s->procUV)->num_hopsU = image_size_UV;
        (&s->procUV)->num_hopsV = image_size_UV;
        lhe_advanced_read_file_symbols3 (s, &s->procY, (&s->lheY)->hops, 0, 1, 1, BASIC_LHE, 0);
        lhe_advanced_read_file_symbols3 (s, &s->procUV, (&s->lheU)->hops, 0, 1, 1, BASIC_LHE, 1);            
        lhe_advanced_read_file_symbols3 (s, &s->procUV, (&s->lheV)->hops, 0, 1, 1, BASIC_LHE, 2);
        
        lhe_basic_decode_frame_sequential (s);
    
    }

    if ((ret = av_frame_ref(data, s->frame)) < 0)
        return ret;
    *got_frame = 1;


    return avpkt->size;//Hay que devolver el numero de bytes leidos (puede ser la solución a la doble ejecución del decoder).
}


//==================================================================
// DECODE VIDEO FRAME
//==================================================================
/**
 * Read and decodes MLHE video
 *
 * @param *avctx Codec context
 * @param *data data from file
 * @param *got_frame indicates frame is ready
 * @param *avpkt AV packet
 */ 
static int mlhe_decode_video(AVCodecContext *avctx, void *data, int *got_frame, AVPacket *avpkt)
{    
    uint32_t image_size_Y, image_size_UV;
    int ret;
    
    float compression_factor;
    uint32_t ppp_max_theoric;

    LheHuffEntry he_mesh[LHE_MAX_HUFF_SIZE_MESH];
    LheState *s = avctx->priv_data;
    const uint8_t *lhe_data = avpkt->data;

    struct timeval before , after;

    gettimeofday(&before , NULL);
    
    init_get_bits(&s->gb, lhe_data, avpkt->size * 8);
    
    s->lhe_mode = get_bits(&s->gb, LHE_MODE_SIZE_BITS); 
        
    if (s->lhe_mode==DELTA_MLHE && s->global_frames_count<=0) 
      return -1;
         
    if (s->lhe_mode == DELTA_MLHE) { /*DELTA VIDEO FRAME*/      
        
        image_size_Y = (&s->procY)->width * (&s->procY)->height;
        image_size_UV = (&s->procUV)->width * (&s->procUV)->height; 

        /*(&s->procY)->width = get_bits(&s->gb, 32);
        (&s->procY)->height = get_bits(&s->gb, 32);
        (&s->procUV)->width = ((&s->procY)->width - 1)/s->chroma_factor_width+1;
        (&s->procUV)->height = ((&s->procY)->height - 1)/s->chroma_factor_height+1;

        image_size_Y =  (&s->procY)->width * (&s->procY)->height;
        image_size_UV = (&s->procUV)->width * (&s->procUV)->height;
*/
        //avctx->width = (&s->procY)->width;
        //avctx->height = (&s->procY)->height;
        
        //Allocates frame
        av_frame_unref(s->frame);
        if ((ret = ff_get_buffer(avctx, s->frame, 0)) < 0)
            return ret;
        
        (&s->lheY)->component_prediction = s->frame->data[0];
        (&s->lheU)->component_prediction  = s->frame->data[1];
        (&s->lheV)->component_prediction  = s->frame->data[2];
        
        (&s->lheY)->first_color_block[0] = get_bits(&s->gb, FIRST_COLOR_SIZE_BITS);
        (&s->lheU)->first_color_block[0] = get_bits(&s->gb, FIRST_COLOR_SIZE_BITS);
        (&s->lheV)->first_color_block[0] = get_bits(&s->gb, FIRST_COLOR_SIZE_BITS); 

         //MESH Huffman
        lhe_read_huffman_table(s, he_mesh, LHE_MAX_HUFF_SIZE_MESH, LHE_HUFFMAN_NODE_BITS_MESH, LHE_HUFFMAN_NO_OCCURRENCES_MESH);     
        
        //Calculate compression factor
        ppp_max_theoric = (&s->procY)->theoretical_block_width/SIDE_MIN;
        if (ppp_max_theoric > PPP_MAX) ppp_max_theoric = PPP_MAX;
        compression_factor = (&s->prec)->compression_factor[ppp_max_theoric][s->quality_level];        
       
        lhe_advanced_read_mesh(s, he_mesh, ppp_max_theoric, compression_factor);
        
        lhe_advanced_read_all_file_symbols (s);
        
        mlhe_decode_delta_frame (s, image_size_Y, image_size_UV);
    } 
    else if (s->lhe_mode == ADVANCED_LHE)
    {   
        s->global_frames_count++;
        
        //(&s->procY)->width = get_bits(&s->gb, 32);
        //(&s->procY)->height = get_bits(&s->gb, 32);
        //(&s->procUV)->width = ((&s->procY)->width - 1)/s->chroma_factor_width+1;
        //(&s->procUV)->height = ((&s->procY)->height - 1)/s->chroma_factor_height+1;

        image_size_Y =  (&s->procY)->width * (&s->procY)->height;
        image_size_UV = (&s->procUV)->width * (&s->procUV)->height;

        //avctx->width = (&s->procY)->width;
        //avctx->height = (&s->procY)->height;
        
        //Allocates frame
        av_frame_unref(s->frame);
        if ((ret = ff_get_buffer(avctx, s->frame, 0)) < 0)
            return ret;
        
        //Pointers to different color components
        (&s->lheY)->component_prediction = s->frame->data[0];
        (&s->lheU)->component_prediction  = s->frame->data[1];
        (&s->lheV)->component_prediction  = s->frame->data[2];
        
        (&s->lheY)->first_color_block[0] = get_bits(&s->gb, FIRST_COLOR_SIZE_BITS);
        (&s->lheU)->first_color_block[0] = get_bits(&s->gb, FIRST_COLOR_SIZE_BITS);
        (&s->lheV)->first_color_block[0] = get_bits(&s->gb, FIRST_COLOR_SIZE_BITS); 

        (&s->procY)-> theoretical_block_width = (&s->procY)->width / s->total_blocks_width;    
        (&s->procY)-> theoretical_block_height = (&s->procY)->height / s->total_blocks_height;   
        
        (&s->procUV)-> theoretical_block_width = (&s->procUV)->width / s->total_blocks_width;
        (&s->procUV)-> theoretical_block_height = (&s->procUV)->height / s->total_blocks_height; 
        
        //MESH Huffman
        lhe_read_huffman_table(s, he_mesh, LHE_MAX_HUFF_SIZE_MESH, LHE_HUFFMAN_NODE_BITS_MESH, LHE_HUFFMAN_NO_OCCURRENCES_MESH);
        
        //Read quality level and calculate compression factor
        s->quality_level = get_bits(&s->gb, QL_SIZE_BITS); 
        ppp_max_theoric = (&s->procY)-> theoretical_block_width/SIDE_MIN;
        if (ppp_max_theoric > PPP_MAX) ppp_max_theoric = PPP_MAX;
        compression_factor = (&s->prec)->compression_factor[ppp_max_theoric][s->quality_level];        

        lhe_advanced_read_mesh(s, he_mesh, ppp_max_theoric, compression_factor);

        lhe_advanced_read_all_file_symbols (s);
                
        lhe_advanced_decode_symbols (s, image_size_Y, image_size_UV);
    }   
    

    



    for (int i=0; i < s->total_blocks_height; i++)
    {
        memcpy((&s->procY)->last_advanced_block[i], (&s->procY)->advanced_block[i], sizeof(AdvancedLheBlock) * (s->total_blocks_width));
        memcpy((&s->procUV)->last_advanced_block[i], (&s->procUV)->advanced_block[i], sizeof(AdvancedLheBlock) * (s->total_blocks_width));
    }   
    
    memcpy ((&s->lheY)->last_downsampled_image, (&s->lheY)->downsampled_image, image_size_Y);    
    memcpy ((&s->lheU)->last_downsampled_image, (&s->lheU)->downsampled_image, image_size_UV);
    memcpy ((&s->lheV)->last_downsampled_image, (&s->lheV)->downsampled_image, image_size_UV);
    
    memset((&s->lheY)->downsampled_image, 0, image_size_Y);
    memset((&s->lheU)->downsampled_image, 0, image_size_UV);
    memset((&s->lheV)->downsampled_image, 0, image_size_UV);     
    
    if ((ret = av_frame_ref(data, s->frame)) < 0) 
        return ret;

    gettimeofday(&after , NULL);
    timecount = time_diff(before , after);
    //av_log(NULL, AV_LOG_INFO, "Tiempo en procesar el paquete: %d\n", timecount);

    *got_frame = 1;

    return 0;
}

static av_cold int lhe_decode_close(AVCodecContext *avctx)
{
    LheState *s = avctx->priv_data;

    lhedec_free_tables(s);
    av_log(NULL, AV_LOG_INFO, "Llama a close despues de liberar los arrays\n");
    av_frame_free(&s->frame);

    return 0;
}

static const AVClass decoder_class = {
    .class_name = "lhe decoder",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
    .category   = AV_CLASS_CATEGORY_DECODER,
};

AVCodec ff_lhe_decoder = {
    .name           = "lhe",
    .long_name      = NULL_IF_CONFIG_SMALL("LHE"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_LHE,
    .priv_data_size = sizeof(LheState),
    .init           = lhe_decode_init,
    .close          = lhe_decode_close,
    .decode         = lhe_decode_frame,
    .priv_class     = &decoder_class,
    .pix_fmts       = (const enum AVPixelFormat[]) {AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV422P, AV_PIX_FMT_YUV444P, AV_PIX_FMT_NONE}
};

AVCodec ff_mlhe_decoder = {
    .name           = "mlhe",
    .long_name      = NULL_IF_CONFIG_SMALL("MLHE"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_MLHE,
    .priv_data_size = sizeof(LheState),
    .init           = lhe_decode_init,
    .close          = lhe_decode_close,
    .decode         = mlhe_decode_video,
    .priv_class     = &decoder_class,
    .pix_fmts       = (const enum AVPixelFormat[]) {AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV422P, AV_PIX_FMT_YUV444P, AV_PIX_FMT_NONE}
};
