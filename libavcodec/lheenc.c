/*
 * LHE Basic encoder
 */

/**
 * @file
 * LHE Basic encoder
 */

#include "avcodec.h"
#include "lhe.h"
#include "internal.h"
#include "put_bits.h"
#include "bytestream.h"
#include "siprdata.h"
#include "unistd.h"


#include <string.h>

/**
 * Lhe Context
 * 
 * @p AVClass *class Pointer to AVClass
 * @p LheBasicPrec prec Caches for LHE
 * @p PutBitContext pb Params for putting bits in a file
 * @p int pr_metrics Print or not print perceptual relevance metrics
 * @p int basic_lhe Basic LHE or Advanced LHE
 */                
typedef struct LheContext {
    AVClass *class;    
    LheBasicPrec prec;
    PutBitContext pb;
    LheProcessing procY;
    LheProcessing procUV;
    LheImage lheY;
    LheImage lheU;
    LheImage lheV;
    uint8_t chroma_factor_width;
    uint8_t chroma_factor_height;
    bool pr_metrics;
    bool basic_lhe;
    int ql;
    int down_mode_reconf;
    int down_mode;
    int num_rectangle;
    int active;
    int protection;
    int xini;
    int xfin;
    int yini;
    int yfin;
    uint16_t dif_frames_count;
    int skip_frames;
    Prot_Rectangle protected_rectangles[MAX_RECTANGLES];
    int rectangles_TTL;
    char *rectangle_list;
    //uint8_t down_mode_p;
    bool color;
    bool pr_metrics_active;
    int ql_reconf;
    int skip_frames_reconf;
    //int down_mode_p_reconf;
    bool color_reconf;
    bool pr_metrics_active_reconf;
    uint8_t gop_reconf;
    int frame_count;
} LheContext;

uint8_t *intermediate_downsample_Y, *intermediate_downsample_U, *intermediate_downsample_V;
struct timeval before , after;
double microsec;

static int lhe_free_tables(AVCodecContext *ctx)
{

    uint32_t total_blocks_height, pixels_block;

    LheContext *s = ctx->priv_data;

    pixels_block = ctx->width / HORIZONTAL_BLOCKS;
    total_blocks_height = ctx->height / pixels_block;

    av_free((&s->lheY)->buffer3);
    av_free((&s->lheU)->buffer3);
    av_free((&s->lheV)->buffer3);
    av_free((&s->lheY)->hops);
    av_free((&s->lheU)->hops);
    av_free((&s->lheV)->hops);
    av_free((&s->lheY)->first_color_block);
    av_free((&s->lheU)->first_color_block);
    av_free((&s->lheV)->first_color_block);

    for (int i=0; i < total_blocks_height; i++)
    {
        av_free((&s->procY)->basic_block[i]);
    }

    av_free((&s->procY)->basic_block);
        
    for (int i=0; i < total_blocks_height; i++)
    {
        av_free((&s->procUV)->basic_block[i]);
    }

    av_free((&s->procUV)->basic_block);

    if (strcmp((ctx->codec)->name, "lhe") == 0) {

        if (s->basic_lhe == 0) {
            
            av_free(intermediate_downsample_Y);
            av_free(intermediate_downsample_U);
            av_free(intermediate_downsample_V);

            for (int i=0; i < total_blocks_height+1; i++)
            {
                av_free((&s->procY)->buffer_perceptual_relevance_x[i]);
            }

            av_free((&s->procY)->buffer_perceptual_relevance_x);
            
            for (int i=0; i < total_blocks_height+1; i++)
            {
                av_free((&s->procY)->buffer_perceptual_relevance_y[i]);
            }

            av_free((&s->procY)->buffer_perceptual_relevance_y);
        
            for (int i=0; i < total_blocks_height+1; i++)
            {
                av_free((&s->procY)->buffer1_perceptual_relevance_x[i]);
            }

            av_free((&s->procY)->buffer1_perceptual_relevance_x);
            
            for (int i=0; i < total_blocks_height+1; i++)
            {
                av_free((&s->procY)->buffer1_perceptual_relevance_y[i]);
            }

            av_free((&s->procY)->buffer1_perceptual_relevance_y);

            //Advanced blocks            
            for (int i=0; i < total_blocks_height; i++)
            {
                av_free((&s->procY)->advanced_block[i]);
            }

            av_free((&s->procY)->advanced_block);
            
            for (int i=0; i < total_blocks_height; i++)
            {
                av_free((&s->procUV)->advanced_block[i]);
            }

            av_free((&s->procUV)->advanced_block);
            
            av_free((&s->lheY)->downsampled_image);
            av_free((&s->lheU)->downsampled_image);
            av_free((&s->lheV)->downsampled_image);

        }

    } else if (strcmp((ctx->codec)->name, "mlhe") == 0) {

        av_free((&s->lheY)->delta);
        av_free((&s->lheU)->delta);
        av_free((&s->lheV)->delta);

        av_free(intermediate_downsample_Y);
        av_free(intermediate_downsample_U);
        av_free(intermediate_downsample_V);
            
        for (int i=0; i < total_blocks_height+1; i++)
        {
            av_free((&s->procY)->buffer_perceptual_relevance_x[i]);
        }

        av_free((&s->procY)->buffer_perceptual_relevance_x);
        
        for (int i=0; i < total_blocks_height+1; i++)
        {
            av_free((&s->procY)->buffer_perceptual_relevance_y[i]);
        }

        av_free((&s->procY)->buffer_perceptual_relevance_y);
    
        for (int i=0; i < total_blocks_height+1; i++)
        {
            av_free((&s->procY)->buffer1_perceptual_relevance_x[i]);
        }

        av_free((&s->procY)->buffer1_perceptual_relevance_x);
        
        for (int i=0; i < total_blocks_height+1; i++)
        {
            av_free((&s->procY)->buffer1_perceptual_relevance_y[i]);
        }

        av_free((&s->procY)->buffer1_perceptual_relevance_y);
        
        for (int i=0; i < total_blocks_height+1; i++)
        {
            av_free((&s->procY)->prx_movement[i]);
            av_free((&s->procY)->pry_movement[i]);
        }

        av_free((&s->procY)->prx_movement);
        av_free((&s->procY)->pry_movement);

        //Advanced blocks
        for (int i=0; i < total_blocks_height; i++)
        {
            av_free((&s->procY)->buffer_advanced_block[i]);
        }

        av_free((&s->procY)->buffer_advanced_block);
        
        for (int i=0; i < total_blocks_height; i++)
        {
            av_free((&s->procUV)->buffer_advanced_block[i]);
        }

        av_free((&s->procUV)->buffer_advanced_block);

        av_free((&s->lheY)->downsampled_image);
        av_free((&s->lheU)->downsampled_image);
        av_free((&s->lheV)->downsampled_image);

        av_free((&s->lheY)->buffer1);
        av_free((&s->lheU)->buffer1);
        av_free((&s->lheV)->buffer1);

        av_free((&s->lheY)->buffer2);
        av_free((&s->lheU)->buffer2);
        av_free((&s->lheV)->buffer2);

        for (int i=0; i < total_blocks_height; i++)
        {
            av_free((&s->procY)->buffer1_advanced_block[i]);
        }

        av_free((&s->procY)->buffer1_advanced_block);
        
        for (int i=0; i < total_blocks_height; i++)
        {
            av_free((&s->procUV)->buffer1_advanced_block[i]);
        }

        av_free((&s->procUV)->buffer1_advanced_block);

    }

    return 0;
}

static int lhe_alloc_tables(AVCodecContext *ctx, LheContext *s)
{

    uint32_t image_size_Y, image_size_UV;
    uint32_t total_blocks_width, total_blocks_height, pixels_block;

    image_size_Y = ctx->width * ctx->height;
    image_size_UV = ctx->width/s->chroma_factor_width * ctx->height/s->chroma_factor_height;

    total_blocks_width = HORIZONTAL_BLOCKS;
    pixels_block = ctx->width / HORIZONTAL_BLOCKS;
    total_blocks_height = ctx->height / pixels_block;

    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->hops, image_size_Y, sizeof(uint8_t), fail);
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->hops, image_size_UV, sizeof(uint8_t), fail);
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->hops, image_size_UV, sizeof(uint8_t), fail);
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->first_color_block, image_size_Y, sizeof(uint8_t), fail);
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->first_color_block, image_size_UV, sizeof(uint8_t), fail);
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->first_color_block, image_size_UV, sizeof(uint8_t), fail);

    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->basic_block, total_blocks_height, sizeof(BasicLheBlock *), fail);
        
    for (int i=0; i < total_blocks_height; i++)
    {
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->basic_block[i], total_blocks_width, sizeof(BasicLheBlock), fail);
    }
        
    FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procUV)->basic_block, total_blocks_height, sizeof(BasicLheBlock *), fail);
        
    for (int i=0; i < total_blocks_height; i++)
    {
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procUV)->basic_block[i], total_blocks_width, sizeof(BasicLheBlock), fail);
    }

    if (strcmp((ctx->codec)->name, "lhe") == 0) {

        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->component_prediction, image_size_Y, sizeof(uint8_t), fail);
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->component_prediction, image_size_UV, sizeof(uint8_t), fail);
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->component_prediction, image_size_UV, sizeof(uint8_t), fail); 
        
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->buffer3, image_size_Y, sizeof(uint8_t), fail);
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->buffer3, image_size_UV, sizeof(uint8_t), fail);
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->buffer3, image_size_UV, sizeof(uint8_t), fail);

        if (s->basic_lhe == 0) {

            FF_ALLOC_ARRAY_OR_GOTO(s, intermediate_downsample_Y, image_size_Y, sizeof(uint8_t), fail);
            FF_ALLOC_ARRAY_OR_GOTO(s, intermediate_downsample_U, image_size_UV, sizeof(uint8_t), fail);
            FF_ALLOC_ARRAY_OR_GOTO(s, intermediate_downsample_V, image_size_UV, sizeof(uint8_t), fail);
            
            FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->buffer_perceptual_relevance_x, (total_blocks_height+1), sizeof(float*), fail); 
        
            for (int i=0; i<total_blocks_height+1; i++) 
            {
                FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->buffer_perceptual_relevance_x[i], (total_blocks_width+1), sizeof(float), fail);
            }
            
            FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->buffer_perceptual_relevance_y, (total_blocks_height+1), sizeof(float*), fail); 
        
            for (int i=0; i<total_blocks_height+1; i++) 
            {
                FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->buffer_perceptual_relevance_y[i], (total_blocks_width+1), sizeof(float), fail);
            }

            FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->buffer1_perceptual_relevance_x, (total_blocks_height+1), sizeof(float*), fail); 
        
            for (int i=0; i<total_blocks_height+1; i++) 
            {
                FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->buffer1_perceptual_relevance_x[i], (total_blocks_width+1), sizeof(float), fail);
            }
            
            FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->buffer1_perceptual_relevance_y, (total_blocks_height+1), sizeof(float*), fail); 
        
            for (int i=0; i<total_blocks_height+1; i++) 
            {
                FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->buffer1_perceptual_relevance_y[i], (total_blocks_width+1), sizeof(float), fail);
            }                  
        
            //Advanced blocks
            FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->buffer_advanced_block, total_blocks_height, sizeof(AdvancedLheBlock *), fail); 
            
            for (int i=0; i < total_blocks_height; i++)
            {
                FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->buffer_advanced_block[i], total_blocks_width, sizeof(AdvancedLheBlock), fail); 
            }
            
            FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procUV)->buffer_advanced_block, total_blocks_height, sizeof(AdvancedLheBlock *), fail); 
            
            for (int i=0; i < total_blocks_height; i++)
            {
                FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procUV)->buffer_advanced_block[i], total_blocks_width, sizeof(AdvancedLheBlock), fail); 
            }
            
            FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->downsampled_image, image_size_Y, sizeof(uint8_t), fail); 
            FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->downsampled_image, image_size_UV, sizeof(uint8_t), fail); 
            FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->downsampled_image, image_size_UV, sizeof(uint8_t), fail);

            FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer1_advanced_block, total_blocks_height, sizeof(AdvancedLheBlock *), fail); 
                
            for (int i=0; i < total_blocks_height; i++)
            {
                FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer1_advanced_block[i], total_blocks_width, sizeof(AdvancedLheBlock), fail); 
            }
            
            FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procUV)->buffer1_advanced_block, total_blocks_height, sizeof(AdvancedLheBlock *), fail); 
            
            for (int i=0; i < total_blocks_height; i++)
            {
                FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procUV)->buffer1_advanced_block[i], total_blocks_width, sizeof(AdvancedLheBlock), fail); 
            } 

        }

    } else if (strcmp((ctx->codec)->name, "mlhe") == 0) {
        
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->delta, image_size_Y, sizeof(uint8_t), fail);
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->delta, image_size_UV, sizeof(uint8_t), fail);
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->delta, image_size_UV, sizeof(uint8_t), fail);

        FF_ALLOC_ARRAY_OR_GOTO(s, intermediate_downsample_Y, image_size_Y, sizeof(uint8_t), fail);
        FF_ALLOC_ARRAY_OR_GOTO(s, intermediate_downsample_U, image_size_UV, sizeof(uint8_t), fail);
        FF_ALLOC_ARRAY_OR_GOTO(s, intermediate_downsample_V, image_size_UV, sizeof(uint8_t), fail);
            
        FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer_perceptual_relevance_x, (total_blocks_height+1), sizeof(float*), fail); 
    
        for (int i=0; i<total_blocks_height+1; i++) 
        {
            FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer_perceptual_relevance_x[i], (total_blocks_width+1), sizeof(float), fail);
        }
        
        FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer_perceptual_relevance_y, (total_blocks_height+1), sizeof(float*), fail); 
    
        for (int i=0; i<total_blocks_height+1; i++) 
        {
            FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer_perceptual_relevance_y[i], (total_blocks_width+1), sizeof(float), fail);
        }

        FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer1_perceptual_relevance_x, (total_blocks_height+1), sizeof(float*), fail); 
    
        for (int i=0; i<total_blocks_height+1; i++) 
        {
            FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer1_perceptual_relevance_x[i], (total_blocks_width+1), sizeof(float), fail);
        }
        
        FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer1_perceptual_relevance_y, (total_blocks_height+1), sizeof(float*), fail); 
    
        for (int i=0; i<total_blocks_height+1; i++) 
        {
            FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer1_perceptual_relevance_y[i], (total_blocks_width+1), sizeof(float), fail);
        } 

        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->prx_movement, (total_blocks_height+1), sizeof(float*), fail); 
    
        for (int i=0; i<total_blocks_height+1; i++) 
        {
            FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->prx_movement[i], (total_blocks_width+1), sizeof(float), fail);
        }
        
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->pry_movement, (total_blocks_height+1), sizeof(float*), fail); 
    
        for (int i=0; i<total_blocks_height+1; i++) 
        {
            FF_ALLOC_ARRAY_OR_GOTO(s, (&s->procY)->pry_movement[i], (total_blocks_width+1), sizeof(float), fail);
        }                 
        
        //Advanced blocks
        FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer_advanced_block, total_blocks_height, sizeof(AdvancedLheBlock *), fail); 
            
        for (int i=0; i < total_blocks_height; i++)
        {
            FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer_advanced_block[i], total_blocks_width, sizeof(AdvancedLheBlock), fail); 
        }
        
        FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procUV)->buffer_advanced_block, total_blocks_height, sizeof(AdvancedLheBlock *), fail); 
        
        for (int i=0; i < total_blocks_height; i++)
        {
            FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procUV)->buffer_advanced_block[i], total_blocks_width, sizeof(AdvancedLheBlock), fail); 
        } 
            
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->downsampled_image, image_size_Y, sizeof(uint8_t), fail); 
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->downsampled_image, image_size_UV, sizeof(uint8_t), fail); 
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->downsampled_image, image_size_UV, sizeof(uint8_t), fail); 

        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->buffer1, image_size_Y, sizeof(uint8_t), fail); 
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->buffer1, image_size_UV, sizeof(uint8_t), fail); 
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->buffer1, image_size_UV, sizeof(uint8_t), fail);

        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->buffer2, image_size_Y, sizeof(uint8_t), fail); 
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->buffer2, image_size_UV, sizeof(uint8_t), fail); 
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->buffer2, image_size_UV, sizeof(uint8_t), fail);
        
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheY)->buffer3, image_size_Y, sizeof(uint8_t), fail);
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheU)->buffer3, image_size_UV, sizeof(uint8_t), fail);
        FF_ALLOC_ARRAY_OR_GOTO(s, (&s->lheV)->buffer3, image_size_UV, sizeof(uint8_t), fail);

        FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer1_advanced_block, total_blocks_height, sizeof(AdvancedLheBlock *), fail); 
            
        for (int i=0; i < total_blocks_height; i++)
        {
            FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procY)->buffer1_advanced_block[i], total_blocks_width, sizeof(AdvancedLheBlock), fail); 
        }
        
        FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procUV)->buffer1_advanced_block, total_blocks_height, sizeof(AdvancedLheBlock *), fail); 
        
        for (int i=0; i < total_blocks_height; i++)
        {
            FF_ALLOCZ_ARRAY_OR_GOTO(s, (&s->procUV)->buffer1_advanced_block[i], total_blocks_width, sizeof(AdvancedLheBlock), fail); 
        } 

    } else {
        goto fail;
    }

    return 0;

    fail:
        lhe_free_tables(ctx);
        return AVERROR(ENOMEM);
}

static void lhe_process_non_persistent_rectangles (LheContext *s)
{

	char *token;
	int i, j, index;

    i = 0;
    j = 0;
    index = 0;

    if (s->rectangle_list == NULL)
        return;

    token = strtok(s->rectangle_list, ",");

	if (token != NULL)
	{
		s->rectangles_TTL = atoi(token);
		token = strtok(NULL, ",");
	}


	while (token != NULL)
	{

		index = i%5;
		if (index == 0){
			s->protected_rectangles[j].protection = atoi(token);
		}
		else if (index == 1)
			s->protected_rectangles[j].xini = atoi(token);
		else if (index == 2)
			s->protected_rectangles[j].xfin = atoi(token);
		else if (index == 3)
			s->protected_rectangles[j].yini = atoi(token);
		else if (index == 4)
		{
			s->protected_rectangles[j].yfin = atoi(token);
			s->protected_rectangles[j].active = 1;
			j++;
		}

		i++;
		token = strtok(NULL, ",");
	}

	if (j > 0)
		s->protected_rectangles[j].active = 0;

	s->rectangle_list[0] = 0;

	if (s->rectangles_TTL > 0)
	{
		s->rectangles_TTL -= 1;
		if (s->rectangles_TTL == 0)	
		{
			s->protected_rectangles[0].active = 0;
		}
	}
}

/**
 * Initializes coder
 * 
 * @param *avctx Pointer to AVCodec context
 */
static av_cold int lhe_encode_init(AVCodecContext *avctx)
{
    LheContext *s = avctx->priv_data;
    uint32_t total_blocks_width, pixels_block, total_blocks_height;

    s->down_mode_reconf = -1;
    s->color_reconf = true;
    //s->down_mode_p_reconf = -1;
    s->pr_metrics_active_reconf = 0;
    s->skip_frames = 0;

    if (avctx->pix_fmt == AV_PIX_FMT_YUV420P)
    {
        s->chroma_factor_width = 2;
        s->chroma_factor_height = 2;
    } else if (avctx->pix_fmt == AV_PIX_FMT_YUV422P) 
    {
        s->chroma_factor_width = 2;
        s->chroma_factor_height = 1;
    } else if (avctx->pix_fmt == AV_PIX_FMT_YUV444P) 
    {
        s->chroma_factor_width = 1;
        s->chroma_factor_height = 1;
    }

    total_blocks_width = HORIZONTAL_BLOCKS;
    pixels_block = avctx->width / HORIZONTAL_BLOCKS;
    total_blocks_height = avctx->height / pixels_block;

    (&s->procY)->width = avctx->width;
    (&s->procY)->height =  avctx->height; 

    (&s->procUV)->width = ((&s->procY)->width - 1)/s->chroma_factor_width + 1;
    (&s->procUV)->height = ((&s->procY)->height - 1)/s->chroma_factor_height + 1;

    (&s->procY)->pr_factor = (&s->procY)->width/128;
    if ((&s->procY)->pr_factor == 0) (&s->procY)->pr_factor = 1;

    (&s->procY)->theoretical_block_width = (&s->procY)->width / total_blocks_width;
    (&s->procY)->theoretical_block_height = (&s->procY)->height / total_blocks_height;       

    (&s->procUV)->theoretical_block_width = (&s->procUV)->width / total_blocks_width;
    (&s->procUV)->theoretical_block_height = (&s->procUV)->height / total_blocks_height;

    lhe_init_cache2(&s->prec);
    lhe_alloc_tables(avctx, s);

    for (int block_y=0; block_y<total_blocks_height; block_y++)      
    {  
        for (int block_x=0; block_x<total_blocks_width; block_x++) 
        {
            lhe_calculate_block_coordinates (&s->procY, &s->procUV,
                                             total_blocks_width, total_blocks_height,
                                             block_x, block_y);
        }
    }

    (&s->procY)->perceptual_relevance_x = (&s->procY)->buffer_perceptual_relevance_x;
    (&s->procY)->perceptual_relevance_y = (&s->procY)->buffer_perceptual_relevance_y;

    (&s->procY)->last_perceptual_relevance_x = (&s->procY)->buffer1_perceptual_relevance_x;
    (&s->procY)->last_perceptual_relevance_y = (&s->procY)->buffer1_perceptual_relevance_y;

    (&s->lheY)->last_downsampled_image = (&s->lheY)->buffer1;
    (&s->lheU)->last_downsampled_image = (&s->lheU)->buffer1;
    (&s->lheV)->last_downsampled_image = (&s->lheV)->buffer1;

    (&s->lheY)->downsampled_player_image = (&s->lheY)->buffer2;
    (&s->lheU)->downsampled_player_image = (&s->lheU)->buffer2;
    (&s->lheV)->downsampled_player_image = (&s->lheV)->buffer2;

    (&s->lheY)->component_prediction = (&s->lheY)->buffer3;
    (&s->lheU)->component_prediction = (&s->lheU)->buffer3;
    (&s->lheV)->component_prediction = (&s->lheV)->buffer3;

    (&s->procY)->advanced_block = (&s->procY)->buffer_advanced_block;
    (&s->procUV)->advanced_block = (&s->procUV)->buffer_advanced_block;

    s->dif_frames_count = s->gop_reconf;

    s->frame_count = 0;

    microsec = 0;

    return 0;
}

//==================================================================
// AUXILIARY FUNCTIONS
//==================================================================
static void mlhe_reconfig (AVCodecContext *avctx, LheContext *s)
{

    if (s->ql_reconf != -1 && s->ql != s->ql_reconf)
        s->ql = s->ql_reconf;
    //if (s->down_mode_reconf != -1 && s->down_mode != s->down_mode_reconf)
    //    s->down_mode = s->down_mode_reconf;
    //if (s->down_mode_p_reconf != -1 && s->down_mode_p != s->down_mode_p_reconf)
    //    s->down_mode_p = s->down_mode_p_reconf;

    if (s->color != s->color_reconf)
        s->color = s->color_reconf;

    lhe_process_non_persistent_rectangles (s);

    if (s->pr_metrics_active != s->pr_metrics_active_reconf)
        s->pr_metrics_active = s->pr_metrics_active_reconf;
    s->skip_frames = s->skip_frames_reconf;
    if (avctx->gop_size != s->gop_reconf)
        avctx->gop_size = s->gop_reconf;

}

static void lhe_advanced_vertical_nearest_neighbour_interpolation (LheProcessing *proc, LheImage *lhe,
                                                                   uint8_t *intermediate_interpolated_image, 
                                                                   int block_x, int block_y) 
{
    float gradient, gradient_0, gradient_1, ppp_y, ppp_0, ppp_1, ppp_2, ppp_3;
    uint32_t xini, xfin_downsampled, yini, yprev_interpolated, yfin_interpolated, yfin_downsampled, downsampled_x_side, downsampled_y_side;
    float yfin_interpolated_float;
    int x, y_sc, i;
    
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
      
    for (x=xini;x<xfin_downsampled;x++)
    {
            gradient=(ppp_2-ppp_0)/(downsampled_y_side-1.0);  
            
            ppp_y=ppp_0;

            //Interpolated y coordinates
            yprev_interpolated = yini; 
            yfin_interpolated_float= yini+ppp_y;

            // bucle for horizontal scanline 
            // scans the downsampled image, pixel by pixel
            for (y_sc=yini;y_sc<yfin_downsampled;y_sc++)
            {            
                yfin_interpolated = yfin_interpolated_float + 0.5;  
                
                for (i=yprev_interpolated;i < yfin_interpolated;i++)
                {
                    intermediate_interpolated_image[i*proc->width+x]=lhe->component_prediction[y_sc*proc->width+x];                  
                }
          
                yprev_interpolated=yfin_interpolated;
                ppp_y+=gradient;
                yfin_interpolated_float+=ppp_y;               
                
            }//y
            ppp_0+=gradient_0;
            ppp_2+=gradient_1;
    }//x
    
}


static void lhe_advanced_horizontal_nearest_neighbour_interpolation (LheProcessing *proc, LheImage *lhe,
                                                                     uint8_t *intermediate_interpolated_image, 
                                                                     int linesize, int block_x, int block_y) 
{
    uint32_t block_height, downsampled_x_side;
    float gradient, gradient_0, gradient_1, ppp_x, ppp_0, ppp_1, ppp_2, ppp_3;
    uint32_t xini, xfin_downsampled, xprev_interpolated, xfin_interpolated, yini, yfin;
    float xfin_interpolated_float;
    int y, x_sc, i;
    
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
    
    for (y=yini; y<yfin; y++)
    {        
        gradient=(ppp_1-ppp_0)/(downsampled_x_side-1.0); 

        ppp_x=ppp_0;
        
        //Interpolated x coordinates
        xprev_interpolated = xini; 
        xfin_interpolated_float= xini+ppp_x;

        for (x_sc=xini; x_sc<xfin_downsampled; x_sc++)
        {
            xfin_interpolated = xfin_interpolated_float + 0.5;            
               
            for (i=xprev_interpolated;i < xfin_interpolated;i++)
            {
                lhe->component_prediction[y*linesize+i]=intermediate_interpolated_image[y*proc->width+x_sc];       
            }
                        
            xprev_interpolated=xfin_interpolated;
            ppp_x+=gradient;
            xfin_interpolated_float+=ppp_x;   
        }//x

        ppp_0+=gradient_0;
        ppp_1+=gradient_1;

    }//y 
}

static void lhe_compute_error_for_psnr (AVCodecContext *avctx, const AVFrame *frame,
                                        uint8_t *component_original_data_Y, uint8_t *component_original_data_U, 
                                        uint8_t *component_original_data_V, uint8_t mode) 
{

    int error= 0;
    LheContext *s;
    LheProcessing *procY, *procUV;
    LheImage *lheY, *lheU, *lheV;
    
    s = avctx->priv_data;
    procY = &s->procY;
    procUV = &s->procUV;
    
    lheY = &s->lheY;
    lheU = &s->lheU;
    lheV = &s->lheV;

    if (mode != BASIC_LHE){

        uint32_t image_size_Y, image_size_UV;
        int total_blocks_height, total_blocks_width, pixels_block;
        uint8_t *intermediate_interpolated_Y, *intermediate_interpolated_U, *intermediate_interpolated_V;

        image_size_Y = procY->width * procY->height;
        image_size_UV = procUV->width * procUV->height;

        total_blocks_width = HORIZONTAL_BLOCKS;
        pixels_block = avctx->width / HORIZONTAL_BLOCKS;
        total_blocks_height = avctx->height / pixels_block;

        intermediate_interpolated_Y = av_calloc(image_size_Y, sizeof(uint8_t));
        intermediate_interpolated_U = av_calloc(image_size_UV, sizeof(uint8_t));
        intermediate_interpolated_V = av_calloc(image_size_UV, sizeof(uint8_t));

        for (int block_y=0; block_y<total_blocks_height; block_y++) 
        {
            for (int block_x=0; block_x<total_blocks_width; block_x++) 
            { 

                lhe_advanced_vertical_nearest_neighbour_interpolation (procY, lheY, intermediate_interpolated_Y,
                                                                                   block_x, block_y);     
                                           
                lhe_advanced_horizontal_nearest_neighbour_interpolation (procY, lheY, intermediate_interpolated_Y, 
                                                                                        frame->linesize[0], block_x, block_y);
                lhe_advanced_vertical_nearest_neighbour_interpolation (procUV, lheU, intermediate_interpolated_U,
                                                                                       block_x, block_y);     
                                
                lhe_advanced_horizontal_nearest_neighbour_interpolation (procUV, lheU, intermediate_interpolated_U, 
                                                                                        frame->linesize[1], block_x, block_y);
                lhe_advanced_vertical_nearest_neighbour_interpolation (procUV, lheV, intermediate_interpolated_V,
                                                                                       block_x, block_y);     
                                
                lhe_advanced_horizontal_nearest_neighbour_interpolation (procUV, lheV, intermediate_interpolated_V, 
                                                                                        frame->linesize[2], block_x, block_y);
            }
        }
    }

    if(frame->data[0]) {
        for(int y=0; y<procY->height; y++){
            for(int x=0; x<procY->width; x++){
                error = component_original_data_Y[y*frame->linesize[0] + x] - lheY->component_prediction[y*procY->width + x];
                error = abs(error);
                avctx->error[0] += error*error;
            }
        }    
    }
    
    if(frame->data[1]) {
        for(int y=0; y<procUV->height; y++){
            for(int x=0; x<procUV->width; x++){
                error = component_original_data_U[y*frame->linesize[1] + x] - lheU->component_prediction[y*procUV->width + x];
                error = abs(error);
                avctx->error[1] += error*error;
            }
        }    
    }
    
    if(frame->data[2]) {
        for(int y=0; y<procUV->height; y++){
            for(int x=0; x<procUV->width; x++){
                error = component_original_data_V[y*frame->linesize[2] + x] - lheV->component_prediction[y*procUV->width + x];
                error = abs(error);
                avctx->error[2] += error*error;
            }
        }    
    }
}

static void print_json_pr_metrics (LheProcessing *procY, int total_blocks_width, int total_blocks_height) 
{
    int i,j;
    
    av_log (NULL, AV_LOG_PANIC, "[");
        
    for (j=0; j<total_blocks_height+1; j++) 
    {
        for (i=0; i<total_blocks_width+1; i++) 
        {  
            if (i==total_blocks_width && j==total_blocks_height) 
            {
                av_log (NULL, AV_LOG_PANIC, "{\"prx\":%.4f, \"pry\":%.4f}", procY->perceptual_relevance_x[j][i], procY->perceptual_relevance_y[j][i]);
            }
            else 
            {
                av_log (NULL, AV_LOG_PANIC, "{\"prx\":%.4f, \"pry\":%.4f},", procY->perceptual_relevance_x[j][i], procY->perceptual_relevance_y[j][i]);
            }
        }
        
    }
    
    av_log (NULL, AV_LOG_PANIC, "]");   
}

/**
 * Downsamples image in x coordinate with different resolution along the block. 
 * Samples are taken using sps with different cadence depending on ppp (pixels per pixel)
 * 
 * @param *proc LHE processing parameters
 * @param *component_original_data original imagage
 * @param *intermediate_downsample_image downsampled image in x coordinate
 * @param block_x block x index
 * @param block_y block y index
 */
static void lhe_advanced_horizontal_downsample_sps (LheProcessing *proc, uint8_t *component_original_data, uint8_t *intermediate_downsample_image,
                                                    int linesize, int block_x, int block_y) 
{
    uint32_t block_height, downsampled_x_side, xini, xdown, xfin_downsampled, yini, yfin;
    float xdown_float;
    float gradient, gradient_0, gradient_1, ppp_x, ppp_0, ppp_1, ppp_2, ppp_3;
    int y, x;

    
    block_height = proc->basic_block[block_y][block_x].block_height;
    downsampled_x_side = proc->advanced_block[block_y][block_x].downsampled_x_side;

    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled;

    yini = proc->basic_block[block_y][block_x].y_ini;
    yfin = proc->basic_block[block_y][block_x].y_fin;
        
    ppp_0=proc->advanced_block[block_y][block_x].ppp_x[TOP_LEFT_CORNER];
    ppp_1=proc->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER];
    ppp_2=proc->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER];
    ppp_3=proc->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER];
    
    //gradient PPPx side a
    gradient_0=(ppp_2-ppp_0)/(block_height-1.0);   
    //gradient PPPx side b
    gradient_1=(ppp_3-ppp_1)/(block_height-1.0);

    for (y=yini; y<yfin; y++)
    {        
        gradient=(ppp_1-ppp_0)/(downsampled_x_side-1.0); 

        ppp_x=ppp_0;
        xdown_float=xini + (ppp_x/2.0);

        for (x=xini; x<xfin_downsampled; x++)
        {
            xdown = xdown_float;
                      
            intermediate_downsample_image[y*proc->width+x]=component_original_data[y*linesize+xdown];

            xdown_float+=ppp_x/2;
            ppp_x+=gradient;
            xdown_float+=ppp_x/2;

        }//x

        ppp_0+=gradient_0;
        ppp_1+=gradient_1;

    }//y
}

//==================================================================
// BASIC LHE FILE
//==================================================================
static void entropic_enc(LheProcessing *proc, LheContext *s, uint8_t *hops, uint16_t block_y_ini, uint16_t block_y_fin, int channel) {

    int xini, yini, xfin_downsampled, yfin_downsampled, pix, dif_pix, mode, condition_length, rlc_length, block_x, inc_x, h0_counter,
            block_y, i, y, x;
    uint8_t hop;
    uint8_t number[9] = { 0,1,1,1,1,1,1,1,1 };
    uint8_t longi[9] = { 8,7,5,3,1,2,4,6,8 };

    mode = HUFFMAN;
    h0_counter = 0;
    condition_length = 7;
    rlc_length = 4;
    block_x = block_y_ini*HORIZONTAL_BLOCKS;
    inc_x = 1;
    hop = 0;

    for (block_y = block_y_ini; block_y < block_y_fin; block_y++) {
        for (i = 0; i < HORIZONTAL_BLOCKS; i++) {

            xini = proc->basic_block[block_y][block_x].x_ini;
            yini = proc->basic_block[block_y][block_x].y_ini;
            
            xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled;          
            yfin_downsampled = proc->advanced_block[block_y][block_x].y_fin_downsampled;

            pix = yini*proc->width+xini;
            dif_pix = proc->width-xfin_downsampled+xini;

            for (y = yini; y < yfin_downsampled; y++) {
                for (x = xini; x < xfin_downsampled; x++) {

                    hop = hops[pix];
                    if (hop == 4) h0_counter++;

                    switch(mode){
                        case HUFFMAN:
                            put_bits(&s->pb, longi[hop], number[hop]);
                            if(hop != 4) h0_counter = 0;
                            if (h0_counter == condition_length) {
                                mode = RLC1;
                                h0_counter = 0;
                            }
                        break;
                        case RLC1:
                            if (hop == 4 && h0_counter == 15) {
                                put_bits(&s->pb, 1, 1);
                                mode = RLC2;
                                rlc_length++;
                                h0_counter = 0;
                            } else if (hop != 4) {
                                put_bits(&s->pb, rlc_length+1, h0_counter);
                                put_bits(&s->pb, longi[hop]-1, number[hop]);
                                h0_counter = 0;
                                mode = HUFFMAN;
                            }
                        break;
                        case RLC2:
                            if (hop == 4 && h0_counter == 31) {
                                put_bits(&s->pb, 1, 1);
                                h0_counter = 0;
                            } else if (hop != 4) {
                                put_bits(&s->pb, rlc_length+1, h0_counter);
                                put_bits(&s->pb, longi[hop]-1, number[hop]);
                                rlc_length = 4;
                                h0_counter = 0;
                                mode = HUFFMAN;
                            }
                        break;
                    }

                    pix++;
                }
                pix+=dif_pix;
            }
            block_x += inc_x;
        }
        block_x = 0;
    }

    if (h0_counter != 0 && mode != HUFFMAN) put_bits(&s->pb, rlc_length+1, h0_counter);

}

static void entropic_enc_basic(LheProcessing *proc, LheContext *s, uint8_t *hops) {

    int pix, mode, h0_counter, condition_length, rlc_length, y, x;
    uint8_t hop;
    uint8_t number[9] = { 0,1,1,1,1,1,1,1,1 };
    uint8_t longi[9] = { 8,7,5,3,1,2,4,6,8 };

    mode = HUFFMAN;
    h0_counter = 0;
    condition_length = 7;
    rlc_length = 4;
    hop = 0;
    pix = 0;

    for (y = 0; y < proc->height; y++) {
        for (x = 0; x < proc->width; x++) {

            hop = hops[pix];
            if (hop == 4) h0_counter++;

            switch(mode){
                case HUFFMAN:
                    put_bits(&s->pb, longi[hop], number[hop]);
                    if(hop != 4) h0_counter = 0;
                    if (h0_counter == condition_length) {
                        mode = RLC1;
                        h0_counter = 0;
                    }
                break;
                case RLC1:
                    if (hop == 4 && h0_counter == 15) {
                        put_bits(&s->pb, 1, 1);
                        mode = RLC2;
                        rlc_length++;
                        h0_counter = 0;
                    } else if (hop != 4) {
                        put_bits(&s->pb, rlc_length+1, h0_counter);
                        put_bits(&s->pb, longi[hop]-1, number[hop]);
                        h0_counter = 0;
                        mode = HUFFMAN;
                    }
                break;
                case RLC2:
                    if (hop == 4 && h0_counter == 31) {
                        put_bits(&s->pb, 1, 1);
                        h0_counter = 0;
                    } else if (hop != 4) {
                        put_bits(&s->pb, rlc_length+1, h0_counter);
                        put_bits(&s->pb, longi[hop]-1, number[hop]);
                        rlc_length = 4;
                        h0_counter = 0;
                        mode = HUFFMAN;
                    }
                break;
            }

            pix++;
        }
    }

    if (h0_counter != 0 && mode != HUFFMAN) put_bits(&s->pb, rlc_length+1, h0_counter);

}


/**
 * Writes BASIC LHE file 
 * 
 * @param *avctx Pointer to AVCodec context
 * @param *pkt Pointer to AVPacket 
 * @param image_size_Y Width x Height of luminance
 * @param image_size_UV Width x Height of chrominances
 * @param total_blocks_width Number of blocks widthwise
 * @param total_blocks_height Number of blocks heightwise
 */
static int lhe_basic_write_file2(AVCodecContext *avctx, AVPacket *pkt, 
                                int image_size_Y, int image_size_UV) {
  
    uint8_t pixel_format;
    uint64_t n_bytes;
        
    LheContext *s;
    LheProcessing *procY, *procUV;
    LheImage *lheY;
    LheImage *lheU;
    LheImage *lheV;
    
    //struct timeval before , after;
    
    s = avctx->priv_data;
    procY = &s->procY;
    procUV = &s->procUV;
    lheY = &s->lheY;
    lheU = &s->lheU;
    lheV = &s->lheV;
    
    //gettimeofday(&before , NULL);
    put_bits(&s->pb, LHE_MODE_SIZE_BITS, BASIC_LHE);
    
    //Pixel format byte
    if (avctx->pix_fmt == AV_PIX_FMT_YUV420P)
    {
        pixel_format = LHE_YUV420;
    } else if (avctx->pix_fmt == AV_PIX_FMT_YUV422P) 
    {
        pixel_format = LHE_YUV422;
    } else if (avctx->pix_fmt == AV_PIX_FMT_YUV444P) 
    {
        pixel_format = LHE_YUV444;
    }
    
    put_bits(&s->pb, PIXEL_FMT_SIZE_BITS, pixel_format);
   
    //save width and height
    put_bits(&s->pb, 16, procY->width);
    put_bits(&s->pb, 16, procY->height);

    //Save first component of each signal 
    put_bits(&s->pb, FIRST_COLOR_SIZE_BITS, lheY->first_color_block[0]);
    put_bits(&s->pb, FIRST_COLOR_SIZE_BITS, lheU->first_color_block[0]);
    put_bits(&s->pb, FIRST_COLOR_SIZE_BITS, lheV->first_color_block[0]);

    entropic_enc_basic(procY, s, lheY->hops);
    entropic_enc_basic(procUV, s, lheU->hops);
    entropic_enc_basic(procUV, s, lheV->hops);

    n_bytes = (put_bits_count(&s->pb)/8)+1;

    pkt->size = n_bytes;
    
    flush_put_bits(&s->pb);

    //gettimeofday(&after , NULL);

    //av_log(NULL, AV_LOG_INFO, "LHE WriteTime %.0lf ", time_diff(before , after));
    
    return n_bytes;
}


//==================================================================
// ADVANCED LHE FILE
//==================================================================


static uint8_t lhe_advanced_translate_pr_into_mesh (float perceptual_relevance) 
{
    uint8_t pr_interval;
    if (perceptual_relevance == PR_QUANT_0) 
    {
        pr_interval = PR_INTERVAL_0;
    } 
    else if (perceptual_relevance == PR_QUANT_1) 
    {
        pr_interval = PR_INTERVAL_1;
    }
    else if (perceptual_relevance == PR_QUANT_2) 
    {
        pr_interval = PR_INTERVAL_2;
    }
    else if (perceptual_relevance == PR_QUANT_3) 
    {
        pr_interval = PR_INTERVAL_3;
    }
    else if (perceptual_relevance == PR_QUANT_5) 
    {
        pr_interval = PR_INTERVAL_4;
    }
   
    return pr_interval;
}

/**
 * Generates Huffman codes for Perceptual Relevance mesh
 * 
 * @param *he_mesh Mesh Huffman parameters
 * @param **perceptual_relevance_x perceptual relevance values in x coordinate
 * @param **perceptual_relevance_y perceptual relevance values in y coordinate
 * @param total_blocks_width number of blocks widthwise
 * @param total_blocks_height number of blocks heightwise
 */
static uint64_t lhe_advanced_gen_huffman_mesh (LheHuffEntry *he_mesh, LheProcessing *proc,
                                               uint32_t total_blocks_width, uint32_t total_blocks_height)
{
    int ret;
    uint64_t n_bits;
    uint8_t  huffman_lengths_mesh [LHE_MAX_HUFF_SIZE_MESH];

    for (int i = 0; i < 5; i++){ 

        proc->pr_quanta_counter[i]++;
    }
    //Generates Huffman length for mesh
    if ((ret = ff_huff_gen_len_table(huffman_lengths_mesh, proc->pr_quanta_counter, 5, 1)) < 0)
        return ret;
    //Fills he_mesh struct with data
    for (int i = 0; i < 5; i++) {
        he_mesh[i].len = huffman_lengths_mesh[i];
        he_mesh[i].count = proc->pr_quanta_counter[i];//symbol_count_mesh[i];
        he_mesh[i].sym = i;
        he_mesh[i].code = 1024; //imposible code to initialize   
    }
    
    //Generates mesh Huffman codes
    n_bits = lhe_generate_huffman_codes(he_mesh, LHE_MAX_HUFF_SIZE_MESH);

    return n_bits; 
}

/**
 * Writes ADVANCED LHE file 
 * 
 * @param *avctx Pointer to AVCodec context
 * @param *pkt Pointer to AVPacket 
 * @param image_size_Y Width x Height of luminance
 * @param image_size_UV Width x Height of chrominances
 * @param total_blocks_width Number of blocks widthwise
 * @param total_blocks_height Number of blocks heightwise
 */
static int lhe_advanced_write_file2(AVCodecContext *avctx, AVPacket *pkt, 
                                   uint32_t image_size_Y, uint32_t image_size_UV, 
                                   uint8_t total_blocks_width, uint8_t total_blocks_height) 
{
  
    uint8_t lhe_mode, pr_interval;
    uint64_t n_bytes;
    int i, block_y, block_x;
            
    LheContext *s;
    LheProcessing *procY;
    LheProcessing *procUV;
    LheImage *lheY;
    LheImage *lheU;
    LheImage *lheV;
    
    LheHuffEntry he_mesh[LHE_MAX_HUFF_SIZE_MESH]; //Struct for mesh Huffman data
    
    //struct timeval before , after;
    
    s = avctx->priv_data;
    procY = &s->procY;
    procUV = &s->procUV;
    lheY = &s->lheY;
    lheU = &s->lheU;
    lheV = &s->lheV;
    
    //gettimeofday(&before , NULL);
    
    //Generates HUffman
    lhe_advanced_gen_huffman_mesh (he_mesh, procY, total_blocks_width, total_blocks_height);
   
    //LHE Mode
    lhe_mode = ADVANCED_LHE; 
    
    //Lhe mode byte
    put_bits(&s->pb, LHE_MODE_SIZE_BITS, lhe_mode);    
    
    //Pixel format byte
    /*if (avctx->pix_fmt == AV_PIX_FMT_YUV420P)
    {
        pixel_format = LHE_YUV420;
    } else if (avctx->pix_fmt == AV_PIX_FMT_YUV422P) 
    {
        pixel_format = LHE_YUV422;
    } else if (avctx->pix_fmt == AV_PIX_FMT_YUV444P) 
    {
        pixel_format = LHE_YUV444;
    }
    
    put_bits(&s->pb, PIXEL_FMT_SIZE_BITS, pixel_format);    
    */  
    //save width and height
    //put_bits32(&s->pb, procY->width);
    //put_bits32(&s->pb, procY->height);    

    put_bits(&s->pb, FIRST_COLOR_SIZE_BITS, lheY->first_color_block[0]);  
    put_bits(&s->pb, FIRST_COLOR_SIZE_BITS, lheU->first_color_block[0]);  
    put_bits(&s->pb, FIRST_COLOR_SIZE_BITS, lheV->first_color_block[0]);  
         
    for (i=0; i<LHE_MAX_HUFF_SIZE_MESH; i++)
    {
        if (he_mesh[i].len==255) he_mesh[i].len=LHE_HUFFMAN_NO_OCCURRENCES_MESH;
        put_bits(&s->pb, LHE_HUFFMAN_NODE_BITS_MESH, he_mesh[i].len);
    }
    
    //Advanced LHE quality level
    put_bits(&s->pb, QL_SIZE_BITS, s->ql); 
   
    //Write mesh. First PRX, then PRY because it eases the decoding task
    //Perceptual Relevance x intervals
    for (block_y=0; block_y<total_blocks_height+1; block_y++) 
    {
        for (block_x=0; block_x<total_blocks_width+1; block_x++) 
        { 
            pr_interval = lhe_advanced_translate_pr_into_mesh(procY->perceptual_relevance_x[block_y][block_x]);
            put_bits(&s->pb, he_mesh[pr_interval].len, he_mesh[pr_interval].code);
        }
    }
    
    //Perceptual relevance y intervals
    for (block_y=0; block_y<total_blocks_height+1; block_y++) 
    {
        for (block_x=0; block_x<total_blocks_width+1; block_x++) 
        { 
            pr_interval = lhe_advanced_translate_pr_into_mesh(procY->perceptual_relevance_y[block_y][block_x]);
            put_bits(&s->pb, he_mesh[pr_interval].len, he_mesh[pr_interval].code);
        }
    }

    entropic_enc(procY, s, lheY->hops, 0, total_blocks_height, 0);
    entropic_enc(procUV, s, lheU->hops, 0, total_blocks_height, 1);
    entropic_enc(procUV, s, lheV->hops, 0, total_blocks_height, 2);

    n_bytes = (put_bits_count(&s->pb)/8)+1;

    pkt->size = n_bytes;
    
    flush_put_bits(&s->pb);

    //gettimeofday(&after , NULL);
    //av_log(NULL, AV_LOG_INFO, "LHE WriteTime %.0lf \n", time_diff(before , after));
    
    return n_bytes;
}          


static int lhe_advanced_write_file2_image(AVCodecContext *avctx, AVPacket *pkt, 
                                   uint32_t image_size_Y, uint32_t image_size_UV, 
                                   uint8_t total_blocks_width, uint8_t total_blocks_height) 
{
  
    uint8_t lhe_mode, pr_interval, pixel_format;
    uint64_t n_bytes;
    int i, block_y, block_x;
            
    LheContext *s;
    LheProcessing *procY;
    LheProcessing *procUV;
    LheImage *lheY;
    LheImage *lheU;
    LheImage *lheV;
    
    LheHuffEntry he_mesh[LHE_MAX_HUFF_SIZE_MESH]; //Struct for mesh Huffman data
    
    //struct timeval before , after;
    
    s = avctx->priv_data;
    procY = &s->procY;
    procUV = &s->procUV;
    lheY = &s->lheY;
    lheU = &s->lheU;
    lheV = &s->lheV;
    
    //gettimeofday(&before , NULL);
    
    //Generates HUffman
    lhe_advanced_gen_huffman_mesh (he_mesh, procY, total_blocks_width, total_blocks_height);
   
    //LHE Mode
    lhe_mode = ADVANCED_LHE; 
    
    //Lhe mode byte
    put_bits(&s->pb, LHE_MODE_SIZE_BITS, lhe_mode);    
    
    //Pixel format byte
    if (avctx->pix_fmt == AV_PIX_FMT_YUV420P)
    {
        pixel_format = LHE_YUV420;
    } else if (avctx->pix_fmt == AV_PIX_FMT_YUV422P) 
    {
        pixel_format = LHE_YUV422;
    } else if (avctx->pix_fmt == AV_PIX_FMT_YUV444P) 
    {
        pixel_format = LHE_YUV444;
    }
    
    put_bits(&s->pb, PIXEL_FMT_SIZE_BITS, pixel_format);    
      
    //save width and height
    put_bits(&s->pb, 16, procY->width);
    put_bits(&s->pb, 16, procY->height);    

    put_bits(&s->pb, FIRST_COLOR_SIZE_BITS, lheY->first_color_block[0]);  
    put_bits(&s->pb, FIRST_COLOR_SIZE_BITS, lheU->first_color_block[0]);  
    put_bits(&s->pb, FIRST_COLOR_SIZE_BITS, lheV->first_color_block[0]);  
         
    for (i=0; i<LHE_MAX_HUFF_SIZE_MESH; i++)
    {
        if (he_mesh[i].len==255) he_mesh[i].len=LHE_HUFFMAN_NO_OCCURRENCES_MESH;
        put_bits(&s->pb, LHE_HUFFMAN_NODE_BITS_MESH, he_mesh[i].len);
    }
    
    //Advanced LHE quality level
    put_bits(&s->pb, QL_SIZE_BITS, s->ql); 
   
    //Write mesh. First PRX, then PRY because it eases the decoding task
    //Perceptual Relevance x intervals
    for (block_y=0; block_y<total_blocks_height+1; block_y++) 
    {
        for (block_x=0; block_x<total_blocks_width+1; block_x++) 
        { 
            pr_interval = lhe_advanced_translate_pr_into_mesh(procY->perceptual_relevance_x[block_y][block_x]);
            put_bits(&s->pb, he_mesh[pr_interval].len, he_mesh[pr_interval].code);
        }
    }
    
    //Perceptual relevance y intervals
    for (block_y=0; block_y<total_blocks_height+1; block_y++) 
    {
        for (block_x=0; block_x<total_blocks_width+1; block_x++) 
        { 
            pr_interval = lhe_advanced_translate_pr_into_mesh(procY->perceptual_relevance_y[block_y][block_x]);
            put_bits(&s->pb, he_mesh[pr_interval].len, he_mesh[pr_interval].code);
        }
    }

    entropic_enc(procY, s, lheY->hops, 0, total_blocks_height, 0);
    entropic_enc(procUV, s, lheU->hops, 0, total_blocks_height, 1);
    entropic_enc(procUV, s, lheV->hops, 0, total_blocks_height, 2);

    n_bytes = (put_bits_count(&s->pb)/8)+1;

    pkt->size = n_bytes;
    
    flush_put_bits(&s->pb);

    //gettimeofday(&after , NULL);
    //av_log(NULL, AV_LOG_INFO, "LHE WriteTime %.0lf \n", time_diff(before , after));
    
    return n_bytes;
}                   

//==================================================================
// BASIC LHE FUNCTIONS
//==================================================================
static void lhe_advanced_encode_block2_image (LheBasicPrec *prec, LheProcessing *proc, LheImage *lhe, uint8_t *original_data,
                                       int total_blocks_width, int block_x, int block_y, int lhe_type, int linesize)
{

    int h1, emin, error, dif_line, dif_pix, pix, pix_original_data, xini, xfin, yini, yfin, 
            num_block, grad, oc, hop0, quantum, hop_value, hop_number, prev_color, ratioY, ratioX;
    bool last_small_hop, small_hop;
    uint8_t *component_original_data, *component_prediction, *hops;
    const int max_h1 = 10;
    const int min_h1 = 4;
    const int start_h1 = min_h1;//(max_h1+min_h1)/2;

    grad = 0;
    
    //gettimeofday(&before , NULL);
    //for (int i = 0; i < 5000; i++){

    xini = proc->basic_block[block_y][block_x].x_ini;
    yini = proc->basic_block[block_y][block_x].y_ini;

    if (lhe_type == DELTA_MLHE) {
        xfin = proc->advanced_block[block_y][block_x].x_fin_downsampled;    
        yfin = proc->advanced_block[block_y][block_x].y_fin_downsampled;
        component_original_data = lhe->delta;
        dif_line = proc->width - xfin + xini;
        dif_pix = dif_line;
        pix_original_data = yini*proc->width + xini;
        pix = yini*proc->width + xini;
    } else if (lhe_type == ADVANCED_LHE) {        
        xfin = proc->advanced_block[block_y][block_x].x_fin_downsampled;    
        yfin = proc->advanced_block[block_y][block_x].y_fin_downsampled;
        component_original_data = lhe->downsampled_image;
        dif_line = proc->width - xfin + xini;
        dif_pix = dif_line;
        pix_original_data = yini*proc->width + xini;
        pix = yini*proc->width + xini;
    } else { //BASIC_LHE
        xfin = proc->basic_block[block_y][block_x].x_fin;
        yfin = proc->basic_block[block_y][block_x].y_fin;
        component_original_data = original_data;
        dif_line = linesize - xfin + xini;
        dif_pix = proc->width - xfin + xini;
        pix_original_data = yini*linesize + xini;
        pix = yini*proc->width + xini;
    }

    ratioY = 1;
    if (block_x > 0){
        ratioY = 1000*(proc->advanced_block[block_y][block_x-1].y_fin_downsampled - proc->basic_block[block_y][block_x-1].y_ini)/(yfin - yini);
    }

    ratioX = 1;
    if (block_y > 0){
        ratioX = 1000*(proc->advanced_block[block_y-1][block_x].x_fin_downsampled - proc->basic_block[block_y-1][block_x].x_ini)/(xfin - xini);
    }

    component_prediction = lhe->component_prediction;
    hops = lhe->hops;
    h1 = start_h1;
    last_small_hop = true;//true; //last hop was small
    small_hop = false;//true;//current hop is small
    emin = 255;//error min
    error = 0;//computed error
    hop0 = 0; //prediction
    hop_value = 0;//data from cache
    hop_number = 4;// final assigned hop
    num_block = block_y * total_blocks_width + block_x;
    oc = component_original_data[pix_original_data];//original color
    if (num_block == 0) lhe->first_color_block[num_block]=oc;
    if (block_x == 0 && block_y == 0) prev_color = oc;
    else if (block_x == 0) prev_color = component_prediction[(proc->advanced_block[block_y-1][block_x].y_fin_downsampled-1)*proc->width+xini];
    else if (block_y == 0) prev_color = component_prediction[yini*proc->width + proc->advanced_block[block_y][block_x-1].x_fin_downsampled-1];
    else prev_color = (component_prediction[yini*proc->width + proc->advanced_block[block_y][block_x-1].x_fin_downsampled-1]+component_prediction[(proc->advanced_block[block_y-1][block_x].y_fin_downsampled-1)*proc->width+xini])/2;
    quantum = oc; //final quantum asigned value

    for (int y = yini; y < yfin; y++) {
        int y_prev = ((y-yini)*ratioY/1000)+yini;
        for (int x=xini;x<xfin;x++) {
            int x_prev = ((x-xini)*ratioX/1000)+xini;
            // --------------------- PHASE 1: PREDICTION-------------------------------
            oc=component_original_data[pix_original_data];//original color
            if (y>yini && x>xini && x<(xfin-1)) { //Interior del bloque
                hop0=(prev_color+component_prediction[pix-proc->width+1])>>1;
            } else if (x==xini && y>yini) { //Lateral izquierdo
                if (x > 0) hop0=(component_prediction[y_prev*proc->width + proc->advanced_block[block_y][block_x-1].x_fin_downsampled-1]+component_prediction[pix-proc->width+1])/2;
                else hop0=component_prediction[pix-proc->width];
                last_small_hop = true;
                h1 = start_h1;
            } else if (y == yini) { //Lateral superior y pixel inicial
                if (y >0 && x != xini) hop0=(prev_color + component_prediction[(proc->advanced_block[block_y-1][block_x].y_fin_downsampled-1)*proc->width+x_prev])/2;
                else hop0=prev_color;
            } else { //Lateral derecho
                hop0=(prev_color+component_prediction[pix-proc->width])>>1;
            }

            if (lhe_type != DELTA_MLHE)
            {
                hop0 = hop0 + grad;
                if (hop0 > 255) hop0 = 255;
                else if (hop0 < 0) hop0 = 0;
            }
            
            //-------------------------PHASE 2: HOPS COMPUTATION-------------------------------
            hop_number = 4;// prediction corresponds with hop_number=4
            quantum = hop0;//this is the initial predicted quantum, the value of prediction
            small_hop = true;//i supossed initially that hop will be small (3,4,5)
            emin = oc-hop0 ; 
            if (emin<0) emin=-emin;//minimum error achieved
            if (emin>h1/2) { //only enter in computation if emin>threshold
                //positive hops
                if (oc>hop0) {
                    //case hop0 (most frequent)
                    if ((quantum +h1)>255) goto phase3;
                    //case hop1 (frequent)
                    error=emin-h1;
                    if (error<0) error=-error;
                    if (error<emin){
                        hop_number=5;
                        emin=error;
                        quantum+=h1;
                    } else goto phase3;
                    // case hops 6 to 8 (less frequent)
                    for (int i=3;i<6;i++){
                        //cache de 5KB simetrica
                        hop_value=255-prec->cache_hops[255-hop0][h1-4][5-i];//indexes are 2 to 0
                        error=oc-hop_value;
                        if (error<0) error=-error;
                        if (error<emin){
                            hop_number=i+3;
                            emin=error;
                            quantum=hop_value;
                        } else break;
                    }
                }
                //negative hops
                else {
                    //case hop0 (most frequent)
                    if ((quantum - h1)<0)    goto phase3;
                    //case hop1 (frequent)
                    error=emin-h1;
                    if (error<0) error=-error;
                    if (error<emin) {
                        hop_number=3;
                        emin=error;
                        quantum-=h1;
                    } else goto phase3;
                    // case hops 2 to 0 (less frequent)
                    for (int i=2;i>=0;i--) {
                        hop_value=prec->cache_hops[hop0][h1-4][i];//indexes are 2 to 0
                        error=hop_value-oc;
                        if (error<0) error=-error;
                        if (error<emin) {
                            hop_number=i;
                            emin=error;
                            quantum=hop_value;
                        } else break;
                    }
                }
            }//endif emin
            //------------- PHASE 3: assignment of final quantized value --------------------------
            phase3:    
            component_prediction[pix]=quantum;
            prev_color=quantum;
            hops[pix]=hop_number;
            //------------- PHASE 4: h1 logic  --------------------------
            if (hop_number>5 || hop_number<3) small_hop=false; //true by default
            if (small_hop==true && last_small_hop==true) {
                if (h1>min_h1) h1--;
            } else {
                h1=max_h1;
            }

            last_small_hop=small_hop;

            if (hop_number == 5) grad = 1;
            else if (hop_number == 3) grad = -1;
            else if (!small_hop) grad = 0;

            pix++;
            pix_original_data++;
        }
        pix+=dif_pix;
        pix_original_data+=dif_line;
    }

    /*
    }
    gettimeofday(&after , NULL);
    microsec += (time_diff(before , after))/5000;
    */
}


static void lhe_advanced_encode_block2_sequential (LheBasicPrec *prec, LheProcessing *proc, LheImage *lhe, uint8_t *original_data,
                                       int total_blocks_width, int block_x, int block_y, int lhe_type, int linesize)
{

    int h1, emin, error, dif_line, dif_pix, pix, pix_original_data, block_ttl, ratioX, ratioY;
    int oc, hop0, quantum, hop_value, hop_number, prev_color;
    bool last_small_hop, small_hop;
    int xini, xfin, yini, yfin, num_block, grad;
    uint8_t *component_original_data, *component_prediction, *hops;
    const int max_h1 = 10;
    const int min_h1 = 4;
    const int start_h1 = min_h1;//(max_h1+min_h1)/2;

    grad = 0;
    
    //gettimeofday(&before , NULL);
    //for (int i = 0; i < 5000; i++){

    xini = proc->basic_block[block_y][block_x].x_ini;
    yini = proc->basic_block[block_y][block_x].y_ini;

    if (lhe_type == DELTA_MLHE) {
        xfin = proc->advanced_block[block_y][block_x].x_fin_downsampled;    
        yfin = proc->advanced_block[block_y][block_x].y_fin_downsampled;
        component_original_data = lhe->delta;
        dif_line = proc->width - xfin + xini;
        dif_pix = dif_line;
        pix_original_data = yini*proc->width + xini;
        pix = yini*proc->width + xini;
    } else if (lhe_type == ADVANCED_LHE) {        
        xfin = proc->advanced_block[block_y][block_x].x_fin_downsampled;    
        yfin = proc->advanced_block[block_y][block_x].y_fin_downsampled;
        component_original_data = lhe->downsampled_image;
        dif_line = proc->width - xfin + xini;
        dif_pix = dif_line;
        pix_original_data = yini*proc->width + xini;
        pix = yini*proc->width + xini;
    } else { //BASIC_LHE
        xfin = proc->basic_block[block_y][block_x].x_fin;
        yfin = proc->basic_block[block_y][block_x].y_fin;
        component_original_data = original_data;
        dif_line = linesize - xfin + xini;
        dif_pix = proc->width - xfin + xini;
        pix_original_data = yini*linesize + xini;
        pix = yini*proc->width + xini;
    }

    block_ttl = proc->advanced_block[block_y][block_x].block_ttl;

    ratioY = 1;
    if (block_x > 0){
        ratioY = 1000*(proc->advanced_block[block_y][block_x-1].y_fin_downsampled - proc->basic_block[block_y][block_x-1].y_ini)/(yfin - yini);
    }

    ratioX = 1;
    if (block_y > 0){
        ratioX = 1000*(proc->advanced_block[block_y-1][block_x].x_fin_downsampled - proc->basic_block[block_y-1][block_x].x_ini)/(xfin - xini);
    }

    component_prediction = lhe->component_prediction;
    hops = lhe->hops;
    h1 = start_h1;
    last_small_hop = true;//true; //last hop was small
    small_hop = false;//true;//current hop is small
    emin = 255;//error min
    error = 0;//computed error
    hop0 = 0; //prediction
    hop_value = 0;//data from cache
    hop_number = 4;// final assigned hop
    num_block = block_y * total_blocks_width + block_x;
    oc = component_original_data[pix_original_data];//original color

    if (num_block == 0) lhe->first_color_block[num_block]=oc;
    if (block_ttl == 30) {
        if (block_x == 0 && block_y == 0) prev_color = oc;
        else if (block_x == 0) prev_color = lhe->downsampled_player_image[(proc->advanced_block[block_y-1][block_x].y_fin_downsampled-1)*proc->width+xini];
        else if (block_y == 0) prev_color = lhe->downsampled_player_image[yini*proc->width + proc->advanced_block[block_y][block_x-1].x_fin_downsampled-1];
        else prev_color = (lhe->downsampled_player_image[yini*proc->width + proc->advanced_block[block_y][block_x-1].x_fin_downsampled-1]+lhe->downsampled_player_image[(proc->advanced_block[block_y-1][block_x].y_fin_downsampled-1)*proc->width+xini])/2;
    } else {
        if (block_x == 0 && block_y == 0) prev_color = oc;
        else prev_color = 128;
    }
    quantum = oc; //final quantum asigned value

    for (int y = yini; y < yfin; y++) {
        int y_prev = ((y-yini)*ratioY/1000)+yini;
        for (int x=xini;x<xfin;x++) {
            int x_prev = ((x-xini)*ratioX/1000)+xini;
            // --------------------- PHASE 1: PREDICTION-------------------------------
            oc=component_original_data[pix_original_data];//original color
            if (y>yini && x>xini && x<(xfin-1)) { //Interior del bloque
                hop0=(prev_color+component_prediction[pix-proc->width+1])>>1;
            } else if (x==xini && y>yini) { //Lateral izquierdo
                if (x > 0) {
                    if (block_ttl == 30) {
                        hop0=(lhe->downsampled_player_image[y_prev*proc->width + proc->advanced_block[block_y][block_x-1].x_fin_downsampled-1]+component_prediction[pix-proc->width+1])/2;
                    } else {
                        hop0=component_prediction[pix-proc->width];
                    }                
                } 
                else hop0=component_prediction[pix-proc->width];
                last_small_hop = true;
                h1 = start_h1;
            } else if (y == yini) { //Lateral superior y pixel inicial
                if (y >0 && x != xini) {
                    if (block_ttl == 30){
                        hop0=(prev_color + lhe->downsampled_player_image[(proc->advanced_block[block_y-1][block_x].y_fin_downsampled-1)*proc->width+x_prev])/2;
                    } else {
                        hop0=prev_color;
                    }

                }
                else hop0=prev_color;
            } else { //Lateral derecho
                hop0=(prev_color+component_prediction[pix-proc->width])>>1;
            }

            if (block_ttl == 30)//if (lhe_type != DELTA_MLHE) ///////CAMBIO POR BLOQUES IP!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            {
                hop0 = hop0 + grad;
                if (hop0 > 255) hop0 = 255;
                else if (hop0 < 0) hop0 = 0;
            }
            
            //-------------------------PHASE 2: HOPS COMPUTATION-------------------------------
            hop_number = 4;// prediction corresponds with hop_number=4
            quantum = hop0;//this is the initial predicted quantum, the value of prediction
            small_hop = true;//i supossed initially that hop will be small (3,4,5)
            emin = oc-hop0 ; 
            if (emin<0) emin=-emin;//minimum error achieved
            if (emin>h1/2) { //only enter in computation if emin>threshold
                //positive hops
                if (oc>hop0) {
                    //case hop0 (most frequent)
                    if ((quantum +h1)>255) goto phase3;
                    //case hop1 (frequent)
                    error=emin-h1;
                    if (error<0) error=-error;
                    if (error<emin){
                        hop_number=5;
                        emin=error;
                        quantum+=h1;
                    } else goto phase3;
                    // case hops 6 to 8 (less frequent)
                    for (int i=3;i<6;i++){
                        //cache de 5KB simetrica
                        hop_value=255-prec->cache_hops[255-hop0][h1-4][5-i];//indexes are 2 to 0
                        error=oc-hop_value;
                        if (error<0) error=-error;
                        if (error<emin){
                            hop_number=i+3;
                            emin=error;
                            quantum=hop_value;
                        } else break;
                    }
                }
                //negative hops
                else {
                    //case hop0 (most frequent)
                    if ((quantum - h1)<0)    goto phase3;
                    //case hop1 (frequent)
                    error=emin-h1;
                    if (error<0) error=-error;
                    if (error<emin) {
                        hop_number=3;
                        emin=error;
                        quantum-=h1;
                    } else goto phase3;
                    // case hops 2 to 0 (less frequent)
                    for (int i=2;i>=0;i--) {
                        hop_value=prec->cache_hops[hop0][h1-4][i];//indexes are 2 to 0
                        error=hop_value-oc;
                        if (error<0) error=-error;
                        if (error<emin) {
                            hop_number=i;
                            emin=error;
                            quantum=hop_value;
                        } else break;
                    }
                }
            }//endif emin
            //------------- PHASE 3: assignment of final quantized value --------------------------
            phase3:    
            component_prediction[pix]=quantum;
            prev_color=quantum;
            hops[pix]=hop_number;
            //------------- PHASE 4: h1 logic  --------------------------
            if (hop_number>5 || hop_number<3) small_hop=false; //true by default
            if (small_hop==true && last_small_hop==true) {
                if (h1>min_h1) h1--;
            } else {
                h1=max_h1;
            }

            last_small_hop=small_hop;

            if (hop_number == 5) grad = 1;
            else if (hop_number == 3) grad = -1;
            else if (!small_hop) grad = 0;

            pix++;
            pix_original_data++;
        }
        pix+=dif_pix;
        pix_original_data+=dif_line;
    }

    /*
    }
    gettimeofday(&after , NULL);
    microsec += (time_diff(before , after))/5000;
    */
}



/**
 * Calls methods to encode sequentially
 */
static void lhe_basic_encode_frame (LheContext *s, const AVFrame *frame, 
                                               uint8_t *component_original_data_Y, uint8_t *component_original_data_U, uint8_t *component_original_data_V)
{

    //Calculate the coordinates for 1 block
    lhe_calculate_block_coordinates (&s->procY, &s->procUV, 1, 1, 0, 0);
    
    //Luminance
    lhe_advanced_encode_block2_image (&s->prec, &s->procY, &s->lheY, component_original_data_Y, 1, 0,  0, BASIC_LHE, frame->linesize[0]); 

    //Crominance U
    lhe_advanced_encode_block2_image (&s->prec, &s->procUV, &s->lheU, component_original_data_U, 1, 0,  0, BASIC_LHE, frame->linesize[1]); 

    //Crominance V
    lhe_advanced_encode_block2_image (&s->prec, &s->procUV, &s->lheV, component_original_data_V, 1, 0,  0, BASIC_LHE, frame->linesize[2]); 
  
}


//==================================================================
// ADVANCED LHE FUNCTIONS
//==================================================================
static void lhe_advanced_compute_pr_lum (LheBasicPrec *prec, LheProcessing *proc,
                                       uint8_t *component_original_data, 
                                       int xini, int xfin, int yini, int yfin, 
                                       int linesize, int block_x, int block_y, 
                                       uint8_t sps_ratio_height, uint8_t sps_ratio_width)
{
         
    //Hops computation.
    int lum_dif, last_lum_dif, pix, y, x, dif;
    int Cx, Cy;
    float PRx, PRy;
    bool lum_sign, last_lum_sign;
    float **perceptual_relevance_x, **perceptual_relevance_y;

    perceptual_relevance_x = proc->perceptual_relevance_x;
    perceptual_relevance_y = proc->perceptual_relevance_y;

    //gettimeofday(&before , NULL);
    //for (int i = 0; i < 1000; i++){

    lum_dif=0;
    last_lum_dif=0;
    lum_sign=true;
    last_lum_sign=true;                        
    Cx=0;
    Cy=0;
    PRx = 0;
    PRy = 0;

    for (y=yini+sps_ratio_height/2;y<yfin;y+=sps_ratio_height)
    {   
        pix = y*linesize+xini;
        if (y>0)//First pixel of an horizontal scanline
        {
            dif=component_original_data[pix]-component_original_data[pix-linesize];
            last_lum_dif=dif;
            if (!(last_lum_sign=(last_lum_dif>=0))) last_lum_dif = -last_lum_dif;

            if (last_lum_dif < QUANT_LUM0) {last_lum_dif = 0;}
            else if (last_lum_dif < QUANT_LUM1) {last_lum_dif = 1;}
            else if (last_lum_dif < QUANT_LUM2) {last_lum_dif = 2;}
            else if (last_lum_dif < QUANT_LUM3) {last_lum_dif = 3;}
            else last_lum_dif = 4;
        }

        for (x=xini;x<xfin;x++)
        {
            dif=0;
            if (x>0) dif=component_original_data[pix]-component_original_data[pix-1];
            lum_dif=dif;
            if (!(lum_sign=(lum_dif>=0))) lum_dif = -lum_dif;
            if (lum_dif < QUANT_LUM0) {lum_dif = 0;}
            else if (lum_dif < QUANT_LUM1) {lum_dif = 1;}
            else if (lum_dif < QUANT_LUM2) {lum_dif = 2;}
            else if (lum_dif < QUANT_LUM3) {lum_dif = 3;}
            else lum_dif = 4;
            if (lum_dif==0) {
                pix++;
                last_lum_dif = lum_dif;
                last_lum_sign = lum_sign;
                continue;
            }

            if ((lum_sign!=last_lum_sign && last_lum_dif!=0) || lum_dif==4) {
                int weight=lum_dif;
                PRx+=weight;
                Cx++;
            }
            last_lum_dif=lum_dif;
            last_lum_sign=lum_sign;
            pix++;
        }
    }

    lum_sign=true;
    lum_dif=0;
    last_lum_sign=true;
    last_lum_dif=0;

    for (x=xini+sps_ratio_width/2;x<xfin;x+=sps_ratio_width)
    {
        pix = yini*linesize+x;    
        if (x>0)
        {
            dif=component_original_data[pix]-component_original_data[pix-1];
            last_lum_dif=dif;
            if (!(last_lum_sign=(last_lum_dif>=0))) last_lum_dif = -last_lum_dif;
            if (last_lum_dif < QUANT_LUM0) {last_lum_dif = 0;}
            else if (last_lum_dif < QUANT_LUM1) {last_lum_dif = 1;}
            else if (last_lum_dif < QUANT_LUM2) {last_lum_dif = 2;}
            else if (last_lum_dif < QUANT_LUM3) {last_lum_dif = 3;}
            else last_lum_dif = 4;
        }
        for (y=yini;y<yfin;y++)
        {  
                                              
            dif=0;
            if (y>0) dif=component_original_data[pix]-component_original_data[pix-linesize];
            lum_dif=dif;
            if (!(lum_sign=(lum_dif>=0))) lum_dif = -lum_dif;
            if (lum_dif < QUANT_LUM0) {lum_dif = 0;}
            else if (lum_dif < QUANT_LUM1) {lum_dif = 1;}
            else if (lum_dif < QUANT_LUM2) {lum_dif = 2;}
            else if (lum_dif < QUANT_LUM3) {lum_dif = 3;}
            else lum_dif = 4;
            if (lum_dif==0) {
                pix += linesize;
                last_lum_dif = lum_dif;
                last_lum_sign=lum_sign;
                continue;
            }

            if ((lum_sign!=last_lum_sign && last_lum_dif!=0) || lum_dif==4) {
                int weight=lum_dif;
                PRy+=weight;
                Cy++;
            }
            last_lum_dif=lum_dif;
            last_lum_sign=lum_sign;
            pix += linesize;
        }
    }

    if (PRx>0) {
        PRx=PRx/(Cx*4); 
    }
    if (PRy>0) {
        PRy=PRy/(Cy*4);
    }
    if (PRx>0.5f) PRx=0.5f;
    if (PRy>0.5f) PRy=0.5f;

    //PR HISTOGRAM EXPANSION
    PRx = (PRx-PR_MIN) / PR_DIF;
            
    //PR QUANTIZATION
    if (PRx < PR_QUANT_1) {
        PRx = PR_QUANT_0;
        proc->pr_quanta_counter[0]++;
    } else if (PRx < PR_QUANT_2) {
        PRx = PR_QUANT_1;
        proc->pr_quanta_counter[1]++;
    } else if (PRx < PR_QUANT_3) {
        PRx = PR_QUANT_2;
        proc->pr_quanta_counter[2]++;
    } else if (PRx < PR_QUANT_4) {
        PRx = PR_QUANT_3;
        proc->pr_quanta_counter[3]++;
    } else {
        PRx = PR_QUANT_5;
        proc->pr_quanta_counter[4]++;
    }       

    perceptual_relevance_x[block_y][block_x] = PRx;
        
    //PR HISTOGRAM EXPANSION
    PRy = (PRy-PR_MIN) / PR_DIF;
    
    //PR QUANTIZATION
    if (PRy < PR_QUANT_1) {
        PRy = PR_QUANT_0;
        proc->pr_quanta_counter[0]++;
    } else if (PRy < PR_QUANT_2) {
        PRy = PR_QUANT_1;
        proc->pr_quanta_counter[1]++;
    } else if (PRy < PR_QUANT_3) {
        PRy = PR_QUANT_2;
        proc->pr_quanta_counter[2]++;
    } else if (PRy < PR_QUANT_4) {
        PRy = PR_QUANT_3;
        proc->pr_quanta_counter[3]++;
    } else {
        PRy = PR_QUANT_5;
        proc->pr_quanta_counter[4]++;
    }
  
    perceptual_relevance_y[block_y][block_x] = PRy;

    //}
    //gettimeofday(&after , NULL);
    //microsec += time_diff(before , after);

}


/**
 * Computes perceptual relevance. 
 * 
 * @param *s LHE Context
 * @param *component_original_data_Y original image data
 * @param linesize rectangle images create a square image in ffmpeg memory. Linesize is width used by ffmpeg in memory
 * @param total_blocks_width total blocks widthwise
 * @param total_blocks_height total blocks heightwise
 */
static void lhe_advanced_compute_perceptual_relevance (LheContext *s, uint8_t *component_original_data_Y,                                           
                                                       int linesize, uint32_t total_blocks_width, uint32_t total_blocks_height) 
{
    
    uint32_t block_width, block_height, half_block_width, half_block_height;

    LheProcessing *proc;
    LheBasicPrec *prec;
    /*
    struct timeval t_ini, t_fin;
    double secs;
    secs = 0;

    gettimeofday(&t_ini, NULL);
    for (int i = 0; i < 1000; i++){
    */
    proc = &s->procY;
    prec = &s->prec;

    for (int i = 0; i < 5; i++) {
        proc->pr_quanta_counter[i] = 0;
    }
    
    block_width = proc->theoretical_block_width;
    block_height = proc->theoretical_block_height;
    half_block_width = (block_width >>1);
    half_block_height = (block_width >>1);

    #pragma omp parallel for schedule(static)
    for (int block_y=0; block_y<total_blocks_height+1; block_y++)      
    {
        for (int block_x=0; block_x<total_blocks_width+1; block_x++) 
        {   

            int xini, xfin, yini, yfin, xini_pr_block, xfin_pr_block, yini_pr_block, yfin_pr_block;
            
            bool modif = false;

            //First LHE Block coordinates
            xini = block_x * block_width;
            xfin = xini + block_width;

            yini = block_y * block_height;
            yfin = yini + block_height;
            
            //PR Blocks coordinates 
            xini_pr_block = xini - half_block_width; 
            
            if (xini_pr_block < 0) 
            {
                xini_pr_block = 0;
            }
            
            xfin_pr_block = xfin - half_block_width;
            
            if (block_x == total_blocks_width) 
            {
                xfin_pr_block = proc->width;
            }    
            
            yini_pr_block = yini - half_block_height;
            
            if (yini_pr_block < 0) 
            {
                yini_pr_block = 0;
            }
            
            yfin_pr_block = yfin - half_block_height;
            
            if (block_y == total_blocks_height)
            {
                yfin_pr_block = proc->height;
            }

            //gettimeofday(&before , NULL);
            //for (int i = 0; i < 1000; i++){

            for (int i = 0; i < MAX_RECTANGLES; i++){
                if (s->protected_rectangles[i].active) {
                    if (!((s->protected_rectangles[i].xfin <= xini_pr_block) || (s->protected_rectangles[i].xini >= xfin_pr_block) || 
                    	(s->protected_rectangles[i].yini >= yfin_pr_block) || (s->protected_rectangles[i].yfin <= yfin_pr_block))) {
                        if (s->protected_rectangles[i].protection == 1) {
                            proc->perceptual_relevance_x[block_y][block_x] = 1;
                            proc->perceptual_relevance_y[block_y][block_x] = 1;
                            modif = true;
                            break;
                        } else {
                            proc->perceptual_relevance_x[block_y][block_x] = 0;
                        	proc->perceptual_relevance_y[block_y][block_x] = 0;
                        	modif = true;
                        }
                    }
                } else {
                	break;
                }
            }

            if (modif == false) {
                //COMPUTE, HISTOGRAM EXPANSION AND QUANTIZATION
                lhe_advanced_compute_pr_lum (prec, proc, component_original_data_Y, 
                                                xini_pr_block, xfin_pr_block, yini_pr_block, yfin_pr_block, 
                                                linesize, block_x, block_y, proc->pr_factor, proc->pr_factor);
            }
        }
    }

    //microsec = 0;
    /*
    }
    gettimeofday(&t_fin, NULL);   
    secs = time_diff(t_ini, t_fin);
    secs = secs/1000.0f;
    av_log(NULL, AV_LOG_INFO, "LHE Compute PR lum and expansion and quant%f \n", secs);
    */
}





static void lhe_advanced_vertical_downsample_sps (LheProcessing *proc, LheImage *lhe, uint8_t *intermediate_downsample_image, int block_x, int block_y) 
{
    
    float ppp_y, ppp_0, ppp_1, ppp_2, ppp_3, gradient, gradient_0, gradient_1, ydown_float;
    uint32_t downsampled_y_side, xini, xfin_downsampled, yini, ydown, yfin_downsampled, downsampled_x_side;
    
    downsampled_y_side = proc->advanced_block[block_y][block_x].downsampled_y_side;
    downsampled_x_side = proc->advanced_block[block_y][block_x].downsampled_x_side;
    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled; //Vertical downsampling is performed after horizontal down. x coord has been already down.  
 
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
      
    for (int x=xini; x < xfin_downsampled;x++)
    {
        gradient=(ppp_2-ppp_0)/(downsampled_y_side-1.0);
        ppp_y=ppp_0; 

        ydown_float=yini + (ppp_y/2.0);
        
        for (int y=yini; y < yfin_downsampled; y++)
        {
            ydown = ydown_float;
            lhe->downsampled_image[y*proc->width+x]=intermediate_downsample_image[ydown*proc->width+x];
      
            ydown_float+=ppp_y/2;
            ppp_y+=gradient;
            ydown_float+=ppp_y/2;
        }//ysc
        ppp_0+=gradient_0;
        ppp_2+=gradient_1;

    }//x
    
}

/**
 * Downsamples image in x coordinate with different resolution along the block. 
 * Final sample is average of ppp samples that it represents 
 * 
 * @param *proc LHE processing parameters
 * @param *component_original_data original image
 * @param *intermediate_downsample intermediate downsampled image in x coordinate
 * @param linesize rectangle images create a square image in ffmpeg memory. Linesize is width used by ffmpeg in memory
 * @param block_x block x index
 * @param block_y block y index
 */
static void lhe_advanced_horizontal_downsample_average (LheProcessing *proc, uint8_t *component_original_data, uint8_t *intermediate_downsample_image,
                                                        int linesize, int block_x, int block_y) 
{
    uint32_t block_height, downsampled_x_side, xini, xdown_prev, xdown_fin, xfin_downsampled, yini, yfin;
    double xdown_prev_float, xdown_fin_float;
    double gradient, gradient_0, gradient_1, ppp_x, ppp_0, ppp_1, ppp_2, ppp_3;
    double component_float, percent;
    uint8_t component;
    
    block_height = proc->basic_block[block_y][block_x].block_height;
    downsampled_x_side = proc->advanced_block[block_y][block_x].downsampled_x_side;

    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled;
 
    yini = proc->basic_block[block_y][block_x].y_ini;
    yfin = proc->basic_block[block_y][block_x].y_fin;  
        
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
        
        xdown_prev_float = xini;
        xdown_prev = xini;
        xdown_fin_float = xini + ppp_x;
        

        for (int x=xini; x<xfin_downsampled; x++)
        {
            xdown_fin = xdown_fin_float;
            
            component_float = 0;
            percent = (1-(xdown_prev_float-xdown_prev));
            
            component_float +=percent*component_original_data[y*linesize+xdown_prev];          
            
            for (int i=xdown_prev+1; i<xdown_fin; i++)
            {
                component_float += component_original_data[y*linesize+i];               
            }
    
            if (xdown_fin_float>xdown_fin)
            {
                percent = xdown_fin_float-xdown_fin;
                component_float += percent * component_original_data[y*linesize+xdown_fin];
            }
                                 
            component_float = component_float / ppp_x;
            
            if (component_float <= 0) component_float = 1;
            if (component_float > 255) component_float = 255;
            
            component = component_float + 0.5;
            
            intermediate_downsample_image[y*proc->width+x] = component;
            
            ppp_x+=gradient;
            xdown_prev = xdown_fin;
            xdown_prev_float = xdown_fin_float;
            xdown_fin_float+=ppp_x;
            
        }//x

        ppp_0+=gradient_0;
        ppp_1+=gradient_1;

    }//y
}

/**
 * Downsamples image in y coordinate with different resolution along the block. 
 * Final sample is average of ppp samples that it represents 
 * 
 * @param *proc LHE processing parameters
 * @param *lhe LHE image arrays
 * @param *intermediate_downsample_image downsampled image in x coordinate
 * @param block_x block x index
 * @param block_y block y index
 */
static void lhe_advanced_vertical_downsample_average (LheProcessing *proc, LheImage *lhe, uint8_t *intermediate_downsample_image, int block_x, int block_y) 
{
    
    double ppp_y, ppp_0, ppp_1, ppp_2, ppp_3, gradient, gradient_0, gradient_1;
    uint32_t downsampled_y_side, xini, xfin_downsampled, yini, ydown_prev, ydown_fin, yfin_downsampled, downsampled_x_side;
    double ydown_prev_float, ydown_fin_float;
    double component_float, percent;
    uint8_t component;
  
    downsampled_y_side = proc->advanced_block[block_y][block_x].downsampled_y_side;
    downsampled_x_side = proc->advanced_block[block_y][block_x].downsampled_x_side;
            
    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled; //Vertical downsampling is performed after horizontal down. x coord has been already down.  
 
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
      
    for (int x=xini; x < xfin_downsampled;x++)
    {
        gradient=(ppp_2-ppp_0)/(downsampled_y_side-1.0);
        ppp_y=ppp_0; 

        ydown_prev = yini;
        ydown_prev_float = yini;
        ydown_fin_float = yini + ppp_y;
        
        for (int y=yini; y < yfin_downsampled; y++)
        {
            ydown_fin = ydown_fin_float;
            
            component_float = 0;
            percent = (1-(ydown_prev_float-ydown_prev));
          
            component_float += percent * intermediate_downsample_image[ydown_prev*proc->width+x];
           
            for (int i=ydown_prev+1; i<ydown_fin; i++)
            {
                component_float += intermediate_downsample_image[i*proc->width+x];
            }
            
            if (ydown_fin_float>ydown_fin)
            {
                percent = ydown_fin_float-ydown_fin;
                component_float += percent *intermediate_downsample_image[ydown_fin*proc->width+x];
            }
            
            component_float = component_float / ppp_y;
            
            if (component_float <= 0) component_float = 1;
            if (component_float > 255) component_float = 255;
            
            component = component_float + 0.5;
            
            lhe->downsampled_image[y*proc->width+x] = component;
            
            //ydown_fin_float+=ppp_y/2;                       
            ppp_y+=gradient;
            ydown_prev = ydown_fin;
            ydown_prev_float = ydown_fin_float;
            ydown_fin_float+=ppp_y;
        }//ysc
        ppp_0+=gradient_0;
        ppp_2+=gradient_1;

    }//x
    
}

static void lhe_advanced_downsample_sps (LheProcessing *proc, LheImage *lhe, uint8_t *component_original_data, int linesize, int block_x, int block_y) 
{
    double pppx_0, pppx_1, pppx_2, pppx_3, pppy_0, pppy_1, pppy_2, pppy_3, pppx, pppy, pppx_a, pppx_b, pppy_a, pppy_b, pppy_c;
    double grad_a_pppx, grad_a_pppy, grad_b_pppx, grad_b_pppy, grad_pppx, grad_pppy, grad_c_pppy;

    uint32_t downsampled_x_side, downsampled_y_side, xini, xfin_downsampled, yfin_downsampled, yini, y_sc, width;
    double ya, x_float, y_float, xa;
    int x, y;
    uint8_t *downsampled_image;

    /*gettimeofday(&before , NULL);
    for (int i = 0; i < 5000; i++){
    */
    downsampled_image = lhe->downsampled_image;
    
    width = proc->width;
    xini = proc->basic_block[block_y][block_x].x_ini;
    xfin_downsampled = proc->advanced_block[block_y][block_x].x_fin_downsampled;

    yini = proc->basic_block[block_y][block_x].y_ini;
    yfin_downsampled = proc->advanced_block[block_y][block_x].y_fin_downsampled;

    pppx_0=proc->advanced_block[block_y][block_x].ppp_x[TOP_LEFT_CORNER];
    pppx_1=proc->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER];
    pppx_2=proc->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER];
    pppx_3=proc->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER];

    pppy_0=proc->advanced_block[block_y][block_x].ppp_y[TOP_LEFT_CORNER];
    pppy_1=proc->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER];
    pppy_2=proc->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER];
    pppy_3=proc->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER];
    
    downsampled_x_side = proc->advanced_block[block_y][block_x].downsampled_x_side;
    downsampled_y_side = proc->advanced_block[block_y][block_x].downsampled_y_side;
    
    downsampled_x_side = xfin_downsampled-xini;
    downsampled_y_side = yfin_downsampled-yini;

    //gradient side a
    grad_a_pppx=(pppx_2 - pppx_0)/(downsampled_y_side-1.0);
    grad_a_pppy=(pppy_2 - pppy_0)/(downsampled_y_side-1.0);

    //gradient side b
    grad_b_pppx=(pppx_3 - pppx_1)/(downsampled_y_side-1.0);
    grad_b_pppy=(pppy_3 - pppy_1)/(downsampled_y_side-1.0);

    grad_c_pppy=(pppy_1 - pppy_0)/(downsampled_x_side-1.0);

    //initialization of ppp at side a and ppp at side b
    pppx_a=pppx_0;
    pppx_b=pppx_1;
    pppy_a=pppy_0;
    pppy_b=pppy_1;
    pppy_c=pppy_0;
    
    ya=yini;
    
    for (y_sc = yini; y_sc < yfin_downsampled; y_sc++)
    {
        pppy_c = pppy_0;

        grad_pppx=(pppx_b-pppx_a)/(downsampled_x_side-1.0); //PPPx gradient between side a and b
        grad_pppy=(pppy_b-pppy_a)/(downsampled_x_side-1.0);

        //initialization of pppx at start of scanline
        pppx=pppx_a;
        xa=xini;
        pppy=pppy_a;
        ya+=pppy_a/2.0;
        
        //original domain without downsampling
        y_float=ya;
        x_float=xa;
        for (int x_sc=xini;x_sc<xfin_downsampled;x_sc++)
        {
            x_float+=pppx/2.0;
            x = x_float;
            y_float = yini+pppy_c/2+((pppy+pppy_c)*(y_sc-yini))/2.0;
            y = y_float;
            //x = xini + pppx_a/2 + ((pppx+pppx_a)*(x_sc-xini))/2;

            downsampled_image[y_sc*width+x_sc] = component_original_data[y*linesize+x];

            x_float+=pppx/2.0;
            pppx+=grad_pppx;
            pppy_c+=grad_c_pppy;
            pppy+=grad_pppy;
        }//x
        pppx_a+=grad_a_pppx;
        pppx_b+=grad_b_pppx;
        
        pppy_b+=grad_b_pppy;
        ya+=pppy_a/2.0;
        pppy_a+=grad_a_pppy;

    }//y

    /*}
    gettimeofday(&after , NULL);
    microsec += (time_diff(before , after));
    */
}

/**
 * LHE advanced encoding
 * 
 * PR to PPP conversion
 * PPP to rectangle shape
 * Elastic Downsampling
 */
static float lhe_advanced_encode (LheContext *s, const AVFrame *frame,  
                                  uint8_t *component_original_data_Y, uint8_t *component_original_data_U, uint8_t *component_original_data_V,
                                  uint32_t total_blocks_width, uint32_t total_blocks_height) 
{
    float compression_factor;    
    uint32_t ppp_max_theoric;
    
    ppp_max_theoric = (&s->procY)->theoretical_block_width/SIDE_MIN;
    if (ppp_max_theoric > PPP_MAX) ppp_max_theoric = PPP_MAX;
    compression_factor = (&s->prec)->compression_factor[ppp_max_theoric][s->ql];

    lhe_advanced_compute_perceptual_relevance (s, component_original_data_Y, frame->linesize[0], total_blocks_width,  total_blocks_height);
/*
    struct timeval t_ini, t_fin;
    double secs;
    gettimeofday(&t_ini, NULL);
    for (int i = 0; i < 1000; i++){
*/
    #pragma omp parallel for schedule(static)
    for (int block_y=0; block_y<total_blocks_height; block_y++) 
    {
        for (int block_x=0; block_x<total_blocks_width; block_x++) 
        {        
            lhe_advanced_perceptual_relevance_to_ppp(&s->procY, &s->procUV, compression_factor, ppp_max_theoric, block_x, block_y);
        }
    }

    for (int block_y=0; block_y<total_blocks_height; block_y++) 
    {
        for (int block_x=0; block_x<total_blocks_width; block_x++) 
        {        
            lhe_advanced_ppp_side_to_rectangle_shape (&s->procY, ppp_max_theoric, block_x, block_y);        
            lhe_advanced_ppp_side_to_rectangle_shape (&s->procUV, ppp_max_theoric, block_x, block_y);

            //Ajuste de adyacencias junio 2018
            if (block_x < total_blocks_width-1 && block_y < total_blocks_height-1){
                
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y][block_x+1].ppp_x[TOP_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y][block_x+1].ppp_x[TOP_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y][block_x+1].ppp_x[TOP_LEFT_CORNER])/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y][block_x+1].ppp_y[TOP_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y][block_x+1].ppp_y[TOP_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y][block_x+1].ppp_y[TOP_LEFT_CORNER])/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y][block_x+1].ppp_x[BOT_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y][block_x+1].ppp_x[BOT_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y][block_x+1].ppp_x[BOT_LEFT_CORNER])/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y][block_x+1].ppp_y[BOT_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y][block_x+1].ppp_y[BOT_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y][block_x+1].ppp_y[BOT_LEFT_CORNER])/2;

                if (((s->procY).advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER]) < ((s->procY).advanced_block[block_y+1][block_x].ppp_x[TOP_LEFT_CORNER]))
                    (&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER]+(&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_LEFT_CORNER])/2;
                if (((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER]) < ((&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_RIGHT_CORNER]))
                    (&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_RIGHT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_RIGHT_CORNER])/2.0f;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER] < (&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER]+(&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_LEFT_CORNER])/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_RIGHT_CORNER])
                    (&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_RIGHT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_RIGHT_CORNER])/2;

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



            switch (s->down_mode)
            {
                case 0:
                    lhe_advanced_downsample_sps (&s->procY, &s->lheY, component_original_data_Y, frame->linesize[0], block_x, block_y);
                    lhe_advanced_downsample_sps (&s->procUV, &s->lheU, component_original_data_U, frame->linesize[1], block_x, block_y); 
                    lhe_advanced_downsample_sps (&s->procUV, &s->lheV, component_original_data_V, frame->linesize[2], block_x, block_y); 
                    break;

                case 1:
                    //LUMINANCE      
                    lhe_advanced_horizontal_downsample_average (&s->procY, component_original_data_Y, intermediate_downsample_Y,
                                                                frame->linesize[0], block_x, block_y);
                                                            
                    lhe_advanced_vertical_downsample_average (&s->procY, &s->lheY, intermediate_downsample_Y, block_x, block_y);
                    
                    //CHROMINANCE U
                    lhe_advanced_horizontal_downsample_average (&s->procUV,component_original_data_U, intermediate_downsample_U,
                                                                frame->linesize[1], block_x, block_y);
                                                            
                    lhe_advanced_vertical_downsample_average (&s->procUV, &s->lheU, intermediate_downsample_U, block_x, block_y);
                    
                    //CHROMINANCE_V
                    lhe_advanced_horizontal_downsample_average (&s->procUV, component_original_data_V, intermediate_downsample_V,
                                                                frame->linesize[2], block_x, block_y);
                                                            
                    lhe_advanced_vertical_downsample_average (&s->procUV, &s->lheV, intermediate_downsample_V, block_x, block_y);

                    break;
                 
                case 2:
                    //LUMINANCE
                    lhe_advanced_horizontal_downsample_sps (&s->procY, component_original_data_Y, intermediate_downsample_Y,
                                                                frame->linesize[0], block_x, block_y);
                                              
                    lhe_advanced_vertical_downsample_sps (&s->procY, &s->lheY, intermediate_downsample_Y,
                                                            block_x, block_y);
                    
                    //CHROMINANCE U
                    lhe_advanced_horizontal_downsample_sps (&s->procUV,component_original_data_U, intermediate_downsample_U,
                                                                frame->linesize[1], block_x, block_y);
                    
                    lhe_advanced_vertical_downsample_sps (&s->procUV, &s->lheU, intermediate_downsample_U,
                                                            block_x, block_y);
                    
                    //CHROMINANCE_V
                    lhe_advanced_horizontal_downsample_sps (&s->procUV, component_original_data_V, intermediate_downsample_V,
                                                                frame->linesize[2], block_x, block_y);

                    lhe_advanced_vertical_downsample_sps (&s->procUV, &s->lheV, intermediate_downsample_V,
                                                            block_x, block_y);

                    break;        

                case 3:
                    //LUMINANCE
                    lhe_advanced_horizontal_downsample_average (&s->procY, component_original_data_Y, intermediate_downsample_Y,
                                                                frame->linesize[0], block_x, block_y);
                                              
                    lhe_advanced_vertical_downsample_sps (&s->procY, &s->lheY, intermediate_downsample_Y,
                                                            block_x, block_y);
                    
                    //CHROMINANCE U
                    lhe_advanced_horizontal_downsample_average (&s->procUV,component_original_data_U, intermediate_downsample_U,
                                                                frame->linesize[1], block_x, block_y);
                    
                    lhe_advanced_vertical_downsample_sps (&s->procUV, &s->lheU, intermediate_downsample_U,
                                                            block_x, block_y);
                    
                    //CHROMINANCE_V
                    lhe_advanced_horizontal_downsample_average (&s->procUV, component_original_data_V, intermediate_downsample_V,
                                                                frame->linesize[2], block_x, block_y);

                    lhe_advanced_vertical_downsample_sps (&s->procUV, &s->lheV, intermediate_downsample_V,
                                                            block_x, block_y);

                    break;            
            }
        }
    }

    for (int i = -(int)total_blocks_height+1; i < (int)total_blocks_width; i++){
        #pragma omp parallel for schedule(static)
        for (int block_y2=total_blocks_height-1; block_y2>=0; block_y2--) 
        {
            int block_y = (block_y2*(total_blocks_height-1))%total_blocks_height;
            int block_x = i + total_blocks_height -1 - block_y;
            if (block_x >= 0 && block_x < total_blocks_width) {
                //LUMINANCE
                lhe_advanced_encode_block2_image (&s->prec, &s->procY, &s->lheY, component_original_data_Y, total_blocks_width, block_x,  block_y, ADVANCED_LHE, 0); 

                //CHROMINANCE U 
                lhe_advanced_encode_block2_image (&s->prec, &s->procUV, &s->lheU, component_original_data_U, total_blocks_width, block_x,  block_y, ADVANCED_LHE, 0);

                //CHROMINANCE V
                lhe_advanced_encode_block2_image (&s->prec, &s->procUV, &s->lheV, component_original_data_V, total_blocks_width, block_x,  block_y, ADVANCED_LHE, 0);
            }
        }
    }

/*
    }
    gettimeofday(&t_fin, NULL);   
    secs = time_diff(t_ini, t_fin);
    secs = secs/1000.0f;
    av_log(NULL, AV_LOG_INFO, "LHE downsample time %f \n", secs);
*/


    return compression_factor;
}

//==================================================================
// LHE VIDEO FUNCTIONS
//==================================================================

static void mlhe_oneshot_calculate_player_image (LheProcessing *proc, LheImage *lhe, int block_x, int block_y)
{
    
    uint32_t xini, xfin, yini, yfin;
    uint32_t downsampled_x_side, downsampled_y_side, last_downsampled_x_side, last_downsampled_y_side;
    int ratioY, ratioX;
    int y, x, yprev, xprev, error_image_int, delta;
    int tramo1, tramo2, signo, dif_tramo;

    tramo1 = 52;
    tramo2 = 204;

    dif_tramo = (tramo2 - tramo1)/2 - tramo1;

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
            delta = lhe->component_prediction[y * proc->width + x];

            delta = delta-128;
            signo = 1;
            if (delta < 0) {
                signo = -1;
                delta = -delta;
            }

            if (delta < tramo1){
                if (signo == 1) error_image_int = lhe->last_downsampled_image[yprev * proc->width + xprev] + delta;
                else error_image_int = lhe->last_downsampled_image[yprev * proc->width + xprev] - delta;
            } else  if (delta <= tramo1+(tramo2-tramo1)/2){
                delta = (delta - tramo1)*2;
                delta += tramo1;
                if (signo == 1) error_image_int = lhe->last_downsampled_image[yprev * proc->width + xprev] + delta;
                else error_image_int = lhe->last_downsampled_image[yprev * proc->width + xprev] - delta;
            } else {
                delta = (delta - dif_tramo)*4;
                delta += tramo2;
                if (signo == 1) error_image_int = lhe->last_downsampled_image[yprev * proc->width + xprev] + delta;
                else error_image_int = lhe->last_downsampled_image[yprev * proc->width + xprev] - delta;
            }

            if (error_image_int > 255) error_image_int = 255;
            else if (error_image_int < 1) error_image_int = 1;

            lhe->downsampled_player_image[y * proc->width + x] = error_image_int;
        }
    }
}

/**
 * Encodes differential frame
 * 
 * @param *s LHE Context
 * @param *frame Frame parameters
 * @param *component_original_data_Y luminance original data
 * @param *component_original_data_U chrominance u original data
 * @param *component_original_data_V chrominance v original data
 * @param total_blocks_width number of blocks widthwise
 * @param total_blocks_height number of blocks heightwise
 */


/**
 * Writes MLHE delta frame 
 * 
 * @param *avctx Pointer to AVCodec context
 * @param *pkt Pointer to AVPacket 
 * @param total_blocks_width Number of blocks widthwise
 * @param total_blocks_height Number of blocks heightwise
 */


//==================================================================
// LHE IMAGE FUNCTIONS
//==================================================================
/**
 * Image encode method
 * 
 * @param *avctx Codec context
 * @param *pkt AV ff_alloc_packet
 * @param *frame AV frame data
 * @param *got_packet indicates packet is ready
 */
static int lhe_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                             const AVFrame *frame, int *got_packet)
{

    uint8_t *component_original_data_Y, *component_original_data_U, *component_original_data_V;
    uint32_t total_blocks_width, total_blocks_height, pixels_block;
    uint32_t image_size_Y, image_size_UV;
    
    uint8_t mode; 
    int ret;   

    //gettimeofday(&before , NULL);

    LheContext *s = avctx->priv_data;

    image_size_Y = (&s->procY)->width * (&s->procY)->height;
    image_size_UV = (&s->procUV)->width * (&s->procUV)->height;
    total_blocks_width = HORIZONTAL_BLOCKS;
    pixels_block = avctx->width / HORIZONTAL_BLOCKS;
    total_blocks_height = avctx->height / pixels_block;

    if (pkt->data == NULL){
        if ((ret = ff_alloc_packet2(avctx, pkt, image_size_Y*3*8, 0)) < 0)
            return ret;
        init_put_bits(&s->pb, pkt->data, image_size_Y*3*8);
    }
    
    //Pointers to different color components
    component_original_data_Y = frame->data[0];
    component_original_data_U = frame->data[1];
    component_original_data_V = frame->data[2];

    mlhe_reconfig(avctx, s);
    
    if (s->basic_lhe) 
    {  
        //BASIC LHE        
        mode = BASIC_LHE;
        total_blocks_height = 1;
        total_blocks_width = 1;
        lhe_basic_encode_frame (s, frame, component_original_data_Y, component_original_data_U, component_original_data_V);
        lhe_basic_write_file2(avctx, pkt,image_size_Y, image_size_UV);
    } 
    else 
    {   
        mode = ADVANCED_LHE;
        lhe_advanced_encode (s, frame, component_original_data_Y, component_original_data_U, component_original_data_V,
                                total_blocks_width, total_blocks_height);
        lhe_advanced_write_file2_image(avctx, pkt, image_size_Y, image_size_UV, total_blocks_width, total_blocks_height); 
    }

    if(avctx->flags&AV_CODEC_FLAG_PSNR){
        lhe_compute_error_for_psnr (avctx, frame, component_original_data_Y, component_original_data_U, component_original_data_V, mode); 
    }
    
    if (s->pr_metrics)
    {
        print_json_pr_metrics(&s->procY, total_blocks_width, total_blocks_height);  
    }

    //av_log (NULL, AV_LOG_INFO, " %.0lf; ",time_diff(before , after));
    //av_log(NULL, AV_LOG_INFO, "CodingTime %.0lf \n", time_diff(before , after));
    pkt->flags |= AV_PKT_FLAG_KEY;
    *got_packet = 1;

    //gettimeofday(&after , NULL);
    //microsec += time_diff(before , after);
 
    return 0;

}

//==================================================================
// ENCODE VIDEO
//==================================================================
/**
 * Video encode method
 * 
 * @param *avctx Codec context
 * @param *pkt AV packet
 * @param *frame AV frame data
 * @param *got_packet indicates packet is ready
 */
static void mlhe_ip_frame_encode (LheContext *s, const AVFrame *frame,                               
                                     uint8_t *component_original_data_Y, uint8_t *component_original_data_U, uint8_t *component_original_data_V,
                                     uint32_t total_blocks_width, uint32_t total_blocks_height)
{

    float compression_factor;
    uint32_t ppp_max_theoric;
    bool twopass_downsampling;

    twopass_downsampling = false;
    ppp_max_theoric = (&s->procY)->theoretical_block_width/SIDE_MIN;
    if (ppp_max_theoric > PPP_MAX) ppp_max_theoric = PPP_MAX;
    compression_factor = (&s->prec)->compression_factor[ppp_max_theoric][s->ql];
    

    //Habria que guardar las PR en dos sitios distintos y uno sería el que utilizaría PR to PPP para
    //no perder la información real del frame cuando estemos en los siguientes.
    lhe_advanced_compute_perceptual_relevance (s, component_original_data_Y, frame->linesize[0],
                                               total_blocks_width,  total_blocks_height);
    
    pr_to_movement(&s->procY);

    //Decission for block I/P
    for (int block_y=0; block_y<total_blocks_height; block_y++)
    {
        for (int block_x=0; block_x<total_blocks_width; block_x++) 
        {
            (&s->procY)->advanced_block[block_y][block_x].block_ttl=30;
            (&s->procUV)->advanced_block[block_y][block_x].block_ttl=30;
            if ((&s->procY)->advanced_block[block_y][block_x].block_ttl <= 0) {
                (&s->procY)->advanced_block[block_y][block_x].block_ttl = 30;     //////SUSTITUIR POR BLOCK GOP EN SU MOMENTO
                (&s->procUV)->advanced_block[block_y][block_x].block_ttl = 30;     //////SUSTITUIR POR BLOCK GOP EN SU MOMENTO
                continue;
            }
            
            if(get_block_movement(&s->procY, block_x, block_y) > 0.26){
                //BLOCK I
                (&s->procY)->advanced_block[block_y][block_x].block_ttl = 30;     //////SUSTITUIR POR BLOCK GOP EN SU MOMENTO
                (&s->procUV)->advanced_block[block_y][block_x].block_ttl = 30;     //////SUSTITUIR POR BLOCK GOP EN SU MOMENTO
            } else {
                //BLOCK P

                //Aqui vamos a programar la mejora de resolución en bloques muy estáticos
                //Si el movimiento es 0 (get_block_movement = =0) y el TTL es menor que 26 (4 frames quieto) then
                // PR = 1 en sus cuatro esquinas (8 PR) ojo!! en el array PR_aux
            }
        }
    }

    #pragma omp parallel for
    for (int block_y=0; block_y<total_blocks_height; block_y++)
    {
        for (int block_x=0; block_x<total_blocks_width; block_x++) 
        {

            //OJO!!! Hay que tocar la funcion pr to ppp para utilizar el array PR_aux
            //si optamos por modificar el PPP en lugar del PR tendremos que tocar las cuatro esquinas
            //del bloque y las PPP de las esquinas de los bloques adyacentes. en total son 12 esquinas.
            lhe_advanced_perceptual_relevance_to_ppp(&s->procY, &s->procUV,
                                                     compression_factor, ppp_max_theoric, 
                                                     block_x, block_y);
        }
    }
           
    for (int block_y=0; block_y<total_blocks_height; block_y++)
    {
        for (int block_x=0; block_x<total_blocks_width; block_x++) 
        { 
            
            lhe_advanced_ppp_side_to_rectangle_shape (&s->procY, ppp_max_theoric, 
                                                      block_x, block_y);        
            lhe_advanced_ppp_side_to_rectangle_shape (&s->procUV, ppp_max_theoric, 
                                                      block_x, block_y);

            //Ajuste de adyacencias junio 2018
            if (block_x < total_blocks_width-1 && block_y < total_blocks_height-1){
                
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y][block_x+1].ppp_x[TOP_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y][block_x+1].ppp_x[TOP_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_x[TOP_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y][block_x+1].ppp_x[TOP_LEFT_CORNER])/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y][block_x+1].ppp_y[TOP_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y][block_x+1].ppp_y[TOP_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_y[TOP_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y][block_x+1].ppp_y[TOP_LEFT_CORNER])/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y][block_x+1].ppp_x[BOT_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y][block_x+1].ppp_x[BOT_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y][block_x+1].ppp_x[BOT_LEFT_CORNER])/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y][block_x+1].ppp_y[BOT_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y][block_x+1].ppp_y[BOT_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y][block_x+1].ppp_y[BOT_LEFT_CORNER])/2;

                if (((s->procY).advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER]) < ((s->procY).advanced_block[block_y+1][block_x].ppp_x[TOP_LEFT_CORNER]))
                    (&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_LEFT_CORNER]+(&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_LEFT_CORNER])/2;
                if (((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER]) < ((&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_RIGHT_CORNER]))
                    (&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_RIGHT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_x[BOT_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y+1][block_x].ppp_x[TOP_RIGHT_CORNER])/2.0f;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER] < (&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_LEFT_CORNER])
                    (&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_LEFT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_LEFT_CORNER]+(&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_LEFT_CORNER])/2;
                if ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER] < (&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_RIGHT_CORNER])
                    (&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_RIGHT_CORNER] = ((&s->procY)->advanced_block[block_y][block_x].ppp_y[BOT_RIGHT_CORNER]+(&s->procY)->advanced_block[block_y+1][block_x].ppp_y[TOP_RIGHT_CORNER])/2;

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
        }
    }

    #pragma omp parallel for
    for (int block_y=0; block_y<total_blocks_height; block_y++)
    {
        for (int block_x=0; block_x<total_blocks_width; block_x++) 
        {
            //LUMINANCE
            lhe_advanced_downsample_sps (&s->procY, &s->lheY, component_original_data_Y, frame->linesize[0], block_x, block_y);
            //lhe_advanced_horizontal_downsample_average (&s->procY, component_original_data_Y, intermediate_downsample_Y, frame->linesize[0], block_x, block_y);
            if (twopass_downsampling == true){
                lhe_advanced_horizontal_downsample_sps (&s->procY, component_original_data_Y, intermediate_downsample_Y, frame->linesize[0], block_x, block_y);
                lhe_advanced_vertical_downsample_sps (&s->procY, &s->lheY, intermediate_downsample_Y, block_x, block_y);
            }
        }
    }

    #pragma omp parallel for
    for (int block_y=0; block_y<total_blocks_height; block_y++)
    {
        for (int block_x=0; block_x<total_blocks_width; block_x++) 
        {
            if ((&s->procY)->advanced_block[block_y][block_x].block_ttl < 30) { //CAMBIAR EL 30 POR BLOCK GOP EN SU MOMENTO
                mlhe_oneshot_adaptres_and_compute_delta (&s->procY, &s->lheY, block_x, block_y);
            } else {
                //Copy the block on the delta image
                int yini = (&s->procY)->basic_block[block_y][block_x].y_ini;
                int yfin = (&s->procY)->advanced_block[block_y][block_x].y_fin_downsampled;
                int xini = (&s->procY)->basic_block[block_y][block_x].x_ini;
                int xside = (&s->procY)->advanced_block[block_y][block_x].downsampled_x_side;
                for (int y = yini; y < yfin; y++) {
                    memcpy((&s->lheY)->delta+(((&s->procY)->width*y)+xini), (&s->lheY)->downsampled_image+(((&s->procY)->width*y)+xini), xside);
                }
            }
        }
    }
    
    for (int i = -(int)total_blocks_height+1; i < (int)total_blocks_width; i++){
        #pragma omp parallel for
        for (int block_y=total_blocks_height-1; block_y>=0; block_y--) 
        {
            int block_x = i + total_blocks_height -1 - block_y;
            if (block_x >= 0 && block_x < total_blocks_width) {
                lhe_advanced_encode_block2_sequential (&s->prec, &s->procY, &s->lheY, component_original_data_Y, total_blocks_width, block_x, block_y, DELTA_MLHE, 0);
                if (s->procY.advanced_block[block_y][block_x].block_ttl < 30) { //CAMBIAR EL 30 POR BLOCK GOP EN SU MOMENTO
                    mlhe_oneshot_calculate_player_image (&s->procY, &s->lheY, block_x, block_y);
                } else {
                    //Copy the block on the delta image
                    int yini = (&s->procY)->basic_block[block_y][block_x].y_ini;
                    int yfin = (&s->procY)->advanced_block[block_y][block_x].y_fin_downsampled;
                    int xini = (&s->procY)->basic_block[block_y][block_x].x_ini;
                    int xside = (&s->procY)->advanced_block[block_y][block_x].downsampled_x_side;
                    for (int y = yini; y < yfin; y++) {
                        memcpy(s->lheY.downsampled_player_image+((s->procY.width*y)+xini), s->lheY.component_prediction+((s->procY.width*y)+xini), xside);
                    }
                }   
            }
        }
    }    

    //CHROMINANCE U            
    #pragma omp parallel for
    for (int block_y=0; block_y<total_blocks_height; block_y++)
    {
        for (int block_x=0; block_x<total_blocks_width; block_x++) 
        {
            lhe_advanced_downsample_sps (&s->procUV, &s->lheU, component_original_data_U, frame->linesize[1], block_x, block_y);
            //lhe_advanced_horizontal_downsample_average (&s->procUV,component_original_data_U, intermediate_downsample_U, frame->linesize[1], block_x, block_y);
            //lhe_advanced_horizontal_downsample_sps (&s->procUV, component_original_data_U, intermediate_downsample_U, frame->linesize[1], block_x, block_y);      
            //lhe_advanced_vertical_downsample_sps (&s->procUV, &s->lheU, intermediate_downsample_U, block_x, block_y);
        }
    }

    #pragma omp parallel for
    for (int block_y=0; block_y<total_blocks_height; block_y++)
    {
        for (int block_x=0; block_x<total_blocks_width; block_x++) 
        {
            if ((&s->procY)->advanced_block[block_y][block_x].block_ttl < 30) { //CAMBIAR EL 30 POR BLOCK GOP EN SU MOMENTO
                mlhe_oneshot_adaptres_and_compute_delta (&s->procUV, &s->lheU, block_x, block_y);
            } else {
                //Copy the block on the delta image
                int yini = (&s->procUV)->basic_block[block_y][block_x].y_ini;
                int yfin = (&s->procUV)->advanced_block[block_y][block_x].y_fin_downsampled;
                int xini = (&s->procUV)->basic_block[block_y][block_x].x_ini;
                int xside = (&s->procUV)->advanced_block[block_y][block_x].downsampled_x_side;
                for (int y = yini; y < yfin; y++) {
                    memcpy((&s->lheU)->delta+(((&s->procUV)->width*y)+xini), (&s->lheU)->downsampled_image+(((&s->procUV)->width*y)+xini), xside);
                }
            }
        }
    }

    for (int i = -(int)total_blocks_height+1; i < (int)total_blocks_width; i++){
        #pragma omp parallel for
        for (int block_y=total_blocks_height-1; block_y>=0; block_y--) 
        {
            int block_x = i + total_blocks_height -1 - block_y;
            if (block_x >= 0 && block_x < total_blocks_width) {
                lhe_advanced_encode_block2_sequential (&s->prec, &s->procUV, &s->lheU, component_original_data_U, total_blocks_width, block_x, block_y, DELTA_MLHE, 0);
                if ((&s->procUV)->advanced_block[block_y][block_x].block_ttl < 30) { //CAMBIAR EL 30 POR BLOCK GOP EN SU MOMENTO                    
                    mlhe_oneshot_calculate_player_image (&s->procUV, &s->lheU, block_x, block_y);
                } else {
                    //Copy the block on the delta image
                    int yini = (&s->procUV)->basic_block[block_y][block_x].y_ini;
                    int yfin = (&s->procUV)->advanced_block[block_y][block_x].y_fin_downsampled;
                    int xini = (&s->procUV)->basic_block[block_y][block_x].x_ini;
                    int xside = (&s->procUV)->advanced_block[block_y][block_x].downsampled_x_side;
                    for (int y = yini; y < yfin; y++) {
                        memcpy(s->lheU.downsampled_player_image+((s->procUV.width*y)+xini), s->lheU.component_prediction+((s->procUV.width*y)+xini), xside);
                    }
                }
            }
        }
    }

    //CHROMINANCE_V
    #pragma omp parallel for
    for (int block_y=0; block_y<total_blocks_height; block_y++)
    {
        for (int block_x=0; block_x<total_blocks_width; block_x++) 
        {
            lhe_advanced_downsample_sps (&s->procUV, &s->lheV, component_original_data_V, frame->linesize[2], block_x, block_y);
            //lhe_advanced_horizontal_downsample_average (&s->procUV, component_original_data_V, intermediate_downsample_V, frame->linesize[2], block_x, block_y);
            //lhe_advanced_horizontal_downsample_sps (&s->procUV, component_original_data_V, intermediate_downsample_V, frame->linesize[2], block_x, block_y);      
            //lhe_advanced_vertical_downsample_sps (&s->procUV, &s->lheV, intermediate_downsample_V, block_x, block_y);
        }
    }

    #pragma omp parallel for
    for (int block_y=0; block_y<total_blocks_height; block_y++)
    {
        for (int block_x=0; block_x<total_blocks_width; block_x++) 
        {
            if ((&s->procY)->advanced_block[block_y][block_x].block_ttl < 30) { //CAMBIAR EL 30 POR BLOCK GOP EN SU MOMENTO
                mlhe_oneshot_adaptres_and_compute_delta (&s->procUV, &s->lheV, block_x, block_y);
            } else {
                //Copy the block on the delta image
                int yini = (&s->procUV)->basic_block[block_y][block_x].y_ini;
                int yfin = (&s->procUV)->advanced_block[block_y][block_x].y_fin_downsampled;
                int xini = (&s->procUV)->basic_block[block_y][block_x].x_ini;
                int xside = (&s->procUV)->advanced_block[block_y][block_x].downsampled_x_side;
                for (int y = yini; y < yfin; y++) {
                    memcpy((&s->lheV)->delta+(((&s->procUV)->width*y)+xini), (&s->lheV)->downsampled_image+(((&s->procUV)->width*y)+xini), xside);
                }
            }            
        }
    }

    for (int i = -(int)total_blocks_height+1; i < (int)total_blocks_width; i++){
        #pragma omp parallel for
        for (int block_y=total_blocks_height-1; block_y>=0; block_y--) 
        {
            int block_x = i + total_blocks_height -1 - block_y;
            if (block_x >= 0 && block_x < total_blocks_width) { 
                lhe_advanced_encode_block2_sequential (&s->prec, &s->procUV, &s->lheV, component_original_data_V, total_blocks_width, block_x, block_y, DELTA_MLHE, 0);
                if ((&s->procUV)->advanced_block[block_y][block_x].block_ttl < 30) { //CAMBIAR EL 30 POR BLOCK GOP EN SU MOMENTO
                    mlhe_oneshot_calculate_player_image (&s->procUV, &s->lheV, block_x, block_y);
                } else {
                    //Copy the block on the delta image
                    int yini = (&s->procUV)->basic_block[block_y][block_x].y_ini;
                    int yfin = (&s->procUV)->advanced_block[block_y][block_x].y_fin_downsampled;
                    int xini = (&s->procUV)->basic_block[block_y][block_x].x_ini;
                    int xside = (&s->procUV)->advanced_block[block_y][block_x].downsampled_x_side;
                    for (int y = yini; y < yfin; y++) {
                        memcpy(s->lheV.downsampled_player_image+((s->procUV.width*y)+xini), s->lheV.component_prediction+((s->procUV.width*y)+xini), xside);
                    }
                }
            }
        }
    }
}

static int mlhe_encode_video_ip(AVCodecContext *avctx, AVPacket *pkt,
                             const AVFrame *frame, int *got_packet)
{

    uint8_t *component_original_data_Y, *component_original_data_U, *component_original_data_V;
    uint32_t total_blocks_width, total_blocks_height, pixels_block;
    uint32_t image_size_Y, image_size_UV;
    int ret;

    LheContext *s = avctx->priv_data;
        
    //gettimeofday(&before , NULL);

    /*if (s->skip_frames > 0) {
        s->frame_count++;
        if (s->frame_count >= s->skip_frames) {
            s->frame_count = 0;
            return 0;
        }
    }*/

    //******************************EN PRUEBA, SI FALLA COMENTARLO********************************************//
    float discard_interval = (float)30.0/(float)(s->skip_frames);
    if (s->skip_frames > 0) {
        s->frame_count++;
        if (s->frame_count >= s->skip_frames) {
            s->frame_count -= discard_interval;
            return 0;
        }
    }

    image_size_Y = (&s->procY)->width * (&s->procY)->height;
    image_size_UV = (&s->procUV)->width * (&s->procUV)->height;

    if (pkt->data == NULL){
        if ((ret = ff_alloc_packet2(avctx, pkt, image_size_Y*3*8, 0)) < 0)
            return ret;
    }
    init_put_bits(&s->pb, pkt->data, image_size_Y*3*8);
    
    total_blocks_width = HORIZONTAL_BLOCKS;
    pixels_block = (&s->procY)->width / HORIZONTAL_BLOCKS;
    total_blocks_height = (&s->procY)->height / pixels_block;
    
    //Pointers to different color components
    component_original_data_Y = frame->data[0];
    component_original_data_U = frame->data[1];
    component_original_data_V = frame->data[2];

    mlhe_reconfig(avctx, s);

    mlhe_ip_frame_encode (s, frame, component_original_data_Y, component_original_data_U, component_original_data_V,
                             total_blocks_width, total_blocks_height);

    //mlhe_advanced_write_delta_frame2(avctx, pkt, total_blocks_width, total_blocks_height); 
    lhe_advanced_write_file2(avctx, pkt, image_size_Y, image_size_UV, total_blocks_width, total_blocks_height); 

    s->lheY.last_downsampled_image = s->lheY.downsampled_player_image;
    s->lheU.last_downsampled_image = s->lheU.downsampled_player_image;
    s->lheV.last_downsampled_image = s->lheV.downsampled_player_image;

    if (s->lheY.downsampled_player_image == s->lheY.buffer1) {
        s->lheY.downsampled_player_image = s->lheY.buffer2;
        s->lheU.downsampled_player_image = s->lheU.buffer2;
        s->lheV.downsampled_player_image = s->lheV.buffer2;
    } else {
        s->lheY.downsampled_player_image = s->lheY.buffer1;
        s->lheU.downsampled_player_image = s->lheU.buffer1;
        s->lheV.downsampled_player_image = s->lheV.buffer1;
    }

    (&s->procY)->last_advanced_block = (&s->procY)->advanced_block;
    (&s->procUV)->last_advanced_block = (&s->procUV)->advanced_block;

    if ((&s->procY)->advanced_block == (&s->procY)->buffer1_advanced_block) {
        (&s->procY)->advanced_block = (&s->procY)->buffer_advanced_block;
        (&s->procUV)->advanced_block = (&s->procUV)->buffer_advanced_block;
    } else {
        (&s->procY)->advanced_block = (&s->procY)->buffer1_advanced_block;
        (&s->procUV)->advanced_block = (&s->procUV)->buffer1_advanced_block;
    }

    (&s->procY)->last_perceptual_relevance_x = (&s->procY)->perceptual_relevance_x;
    (&s->procY)->last_perceptual_relevance_y = (&s->procY)->perceptual_relevance_y;

    if ((&s->procY)->perceptual_relevance_x == (&s->procY)->buffer_perceptual_relevance_x){
        (&s->procY)->perceptual_relevance_x = (&s->procY)->buffer1_perceptual_relevance_x;
        (&s->procY)->perceptual_relevance_y = (&s->procY)->buffer1_perceptual_relevance_y;    
    } else {
        (&s->procY)->perceptual_relevance_x = (&s->procY)->buffer_perceptual_relevance_x;
        (&s->procY)->perceptual_relevance_y = (&s->procY)->buffer_perceptual_relevance_y;    
    }

    pkt->flags |= AV_PKT_FLAG_KEY;
    *got_packet = 1;
    //gettimeofday(&after , NULL);
    //microsec += time_diff(before , after);

    return 0;

}

static int lhe_encode_close(AVCodecContext *avctx)
{

    lhe_free_tables(avctx);
    microsec = microsec/487.0f;
    av_log(NULL, AV_LOG_INFO, "Execution time: %f \n", microsec/1000);
    //av_log(NULL, AV_LOG_INFO, "Numero de fotogramas nulos: %f \n", num_bloques_nulos);
    
    return 0;

}

#define OFFSET(x) offsetof(LheContext, x)
#define VE AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    { "pr_metrics", "Print PR metrics", OFFSET(pr_metrics), AV_OPT_TYPE_BOOL, { .i64 = 0 }, 0, 1, VE },
    { "basic_lhe", "Basic LHE", OFFSET(basic_lhe), AV_OPT_TYPE_BOOL, { .i64 = 0 }, 0, 1, VE },
    { "ql", "Quality level from 0 to 99", OFFSET(ql_reconf), AV_OPT_TYPE_INT, { .i64 = 25 }, 0, 99, VE },
    { "block_gop", "GOP size from 0 to 32000", OFFSET(gop_reconf), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 32000, VE },
    { "down_mode", "0 -> SPS, 1 -> AVG, 2 -> AVGY+SPSX", OFFSET(down_mode), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 3, VE },
    { "num_rectangle", "Number of rectangle to modify", OFFSET(num_rectangle), AV_OPT_TYPE_INT, { .i64 = 4 }, 0, 4, VE },
    { "active", "Avtivate the rectangle", OFFSET(active), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 1, VE },
    { "protection", "Activate protection of the rectangle", OFFSET(protection), AV_OPT_TYPE_INT, { .i64 = 1 }, 0, 1, VE },
    { "xini", "X coordinate for top left corner", OFFSET(xini), AV_OPT_TYPE_INT, { .i64 = 0 }, -100, 30000, VE },
    { "xfin", "X coordinate for bottom right corner", OFFSET(xfin), AV_OPT_TYPE_INT, { .i64 = 0 }, -100, 30000, VE },
    { "yini", "Y coordinate for top left corner", OFFSET(yini), AV_OPT_TYPE_INT, { .i64 = 0 }, -100, 30000, VE },
    { "yfin", "Y coordinate for bottom right corner", OFFSET(yfin), AV_OPT_TYPE_INT, { .i64 = 0 }, -100, 30000, VE },
    { "skip_frames", "Parameter to skip frames to encode", OFFSET(skip_frames_reconf), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 100, VE },
    { NULL },
};

//0,-50,700,-50,700,1,50,100,50,100

static const AVClass lhe_class = {
    .class_name = "LHE Basic encoder",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_lhe_encoder = {
    .name           = "lhe",
    .long_name      = NULL_IF_CONFIG_SMALL("LHE"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_LHE,
    .priv_data_size = sizeof(LheContext),
    .init           = lhe_encode_init,
    .encode2        = lhe_encode_frame,
    .close          = lhe_encode_close,
    .pix_fmts       = (const enum AVPixelFormat[]){
        AV_PIX_FMT_YUV420P, 
        AV_PIX_FMT_YUV422P, 
        AV_PIX_FMT_YUV444P, 
        AV_PIX_FMT_NONE
    },
    .priv_class     = &lhe_class,
};

AVCodec ff_mlhe_encoder = {
    .name           = "mlhe",
    .long_name      = NULL_IF_CONFIG_SMALL("MLHE"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_MLHE,
    .priv_data_size = sizeof(LheContext),
    .init           = lhe_encode_init,
    .encode2        = mlhe_encode_video_ip,
    .close          = lhe_encode_close,
    .pix_fmts       = (const enum AVPixelFormat[]){
        AV_PIX_FMT_YUV420P, 
        AV_PIX_FMT_YUV422P, 
        AV_PIX_FMT_YUV444P, 
        AV_PIX_FMT_NONE
    },
    .priv_class     = &lhe_class,
};
