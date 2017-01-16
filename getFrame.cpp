// to compile:
//gcc -I/usr/ort/lib/ffmpeg-2.8.1/include -L/usr/ort/lib/ffmpeg-2.8.1/lib -Wl,-rpath=/usr/ort/lib/ffmpeg-2.8.1/lib demux.c -o demux -g -lavformat -lavcodec -lavutil

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#define CHECK_ERR(ERR) {if ((ERR)<0) return -1; }

int main(int argc, char *argv[]) {
    av_register_all();
    avcodec_register_all();
    int i = 0;
    AVFormatContext * ctx = NULL;
    AVPixelFormat pixFormat;
    AVCodecID codecID;
    char extension[5];
    bool encode = true;

    if(strcmp(argv[2], "png") == 0 ) {
      pixFormat = AV_PIX_FMT_RGB24;
      codecID = CODEC_ID_PNG;
      sprintf(extension, "png");
    }
    else if(strcmp(argv[2], "ppm") == 0) {
      pixFormat = AV_PIX_FMT_RGB24;
      encode = false;
      sprintf(extension, "ppm");
    }
    else if(strcmp(argv[2], "jpg") == 0) {
      pixFormat = PIX_FMT_YUVJ420P;
      codecID = CODEC_ID_MJPEG;
      sprintf(extension, "jpg");
    }
    else {
      CHECK_ERR(-1);
    }



    int err = avformat_open_input(&ctx, argv[1], NULL, NULL);
    CHECK_ERR(err);
    err = avformat_find_stream_info(ctx, NULL);
    CHECK_ERR(err);
    av_dump_format(ctx, 0, argv[1], 0);

    //find stream
    AVCodec * codec = NULL;
    int strm = av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

    //open codecContext
    AVCodecContext * codecCtx = ctx->streams[strm]->codec;
    codec=avcodec_find_decoder(codecCtx->codec_id);
    err = avcodec_open2(codecCtx, codec, NULL);
    CHECK_ERR(err);

    AVFrame * outputFrame = av_frame_alloc();
    uint8_t *buffer = NULL;
    int numBytes;
    numBytes = avpicture_get_size(pixFormat, codecCtx->width, codecCtx->height);
    buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill((AVPicture*) outputFrame, buffer, pixFormat,
                      codecCtx->width, codecCtx->height);

    SwsContext * swCtx = sws_getContext(codecCtx->width,
        codecCtx->height,
        codecCtx->pix_fmt,
        codecCtx->width,
        codecCtx->height,
        pixFormat,
        SWS_FAST_BILINEAR, 0, 0, 0);

    AVPacket pkt;
    i = 0;
    while(av_read_frame(ctx, &pkt) >= 0)
    {

        if (pkt.stream_index == strm)
        {
            int got = 0;
            AVFrame * frame = av_frame_alloc();
            err = avcodec_decode_video2(codecCtx, frame, &got, &pkt);
            CHECK_ERR(err);

            if (got)
            {
                sws_scale(swCtx, frame->data, frame->linesize, 0, frame->height, outputFrame->data, outputFrame->linesize);

                AVCodec *outCodec = avcodec_find_encoder(codecID);
                AVCodecContext *outCodecCtx = avcodec_alloc_context3(outCodec);
                if (!codecCtx) {
                    return -1;
                }

                if(!encode) {
                  if(++i <= 5) {
                    FILE *pFile;
                    char szFilename[32];
                    int  y;
                    // Open file
                    sprintf(szFilename, "frame%d.ppm", i);
                    pFile=fopen(szFilename, "wb");
                    if(pFile==NULL)
                      return -1;
                    // Write header
                    fprintf(pFile, "P6\n%d %d\n255\n", codecCtx->width, codecCtx->height);
                    // Write pixel data
                    for(y=0; y<codecCtx->height; y++) {
                      fwrite(outputFrame->data[0]+y*outputFrame->linesize[0], 1, codecCtx->width*3, pFile);
                    }
                    // Close file
                    fclose(pFile);
                  }
                }
                else {
                  outCodecCtx->width = codecCtx->width;
                  outCodecCtx->height = codecCtx->height;
                  outCodecCtx->pix_fmt = pixFormat;
                  outCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
                  outCodecCtx->time_base.num = codecCtx->time_base.num;
                  outCodecCtx->time_base.den = codecCtx->time_base.den;

                  if (!outCodec || avcodec_open2(outCodecCtx, outCodec, NULL) < 0) {
                      return -1;
                  }

                  if(++i <= 5) {
                      char szFilename[32];
                      AVPacket outPacket;
                      av_init_packet(&outPacket);
                      outPacket.size = 0;
                      outPacket.data = NULL;
                      int gotFrame = 0;
                      outputFrame->format = pixFormat;
                      outputFrame->height = codecCtx->height;
                      outputFrame->width = codecCtx->width;
                      int ret = avcodec_encode_video2(outCodecCtx, &outPacket, outputFrame, &gotFrame);
                      if (ret >= 0 && gotFrame)
                      {
                          sprintf(szFilename, "frame%d.%s", i, extension);
                          FILE * outPng = fopen(szFilename, "wb");
                          fwrite(outPacket.data, outPacket.size, 1, outPng);
                          fclose(outPng);
                      }
                  }
                }

                avcodec_close(outCodecCtx);
                av_free(outCodecCtx);

            }
            av_free(frame);
        }
    }
}
