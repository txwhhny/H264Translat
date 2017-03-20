extern "C"
{
#include <libavcodec\avcodec.h>
#include <libavutil\frame.h>
#include <basetsd.h>
#include <libavutil\opt.h>
#include <libavutil\imgutils.h>
#include <libswscale\swscale.h>
#include <libavformat\avformat.h>
}
#include <opencv2\opencv.hpp>

#include <iostream>  
#include <stdio.h>  
#include <WINSOCK2.H>  

#pragma  comment(lib,"ws2_32.lib")  

AVCodec * mCodec = NULL;
AVCodecContext * mCodecCtx = NULL;
void initFfmpeg()
{
    av_register_all();
    avcodec_register_all();

    mCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!mCodec)
    {
        printf("avcodec_find_encoder failed");
        return;
    }

    mCodecCtx = avcodec_alloc_context3(mCodec);
    if (!mCodecCtx)
    {
        printf("avcodec_alloc_context3 failed");
        return;
    }
    mCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    mCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (avcodec_open2(mCodecCtx, mCodec, NULL) < 0)
    {
        avcodec_free_context(&mCodecCtx);
        printf("avcodec_open2 failed");
        return;
    }
}

AVPacket * mPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
AVFrame * mFrameSrc = av_frame_alloc();
AVFrame * mFrameYUV = av_frame_alloc();
AVFrame * mFrameRGB = av_frame_alloc();
SwsContext * mImgConvertCtx = NULL;
int got_picture = 0;
cv::Mat mat;
void decodeFfmpeg(char * buf, int len)
{
    if (len == 0)   return;

    av_init_packet(mPacket);
    mPacket->size = len;
    mPacket->data = (uint8_t*)buf;
    // 解码
    int ret = avcodec_decode_video2(mCodecCtx, mFrameSrc, &got_picture, mPacket);
    if (ret < 0)
    {
        printf("Decode Error.\n");
        return;
    }
    if (got_picture)
    {
        if (mFrameYUV->data[0] == NULL)
        {
            uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, mCodecCtx->width, mCodecCtx->height));
            avpicture_fill((AVPicture *)mFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, mCodecCtx->width, mCodecCtx->height);
        }
        if (NULL == mImgConvertCtx)
        {
            mImgConvertCtx = sws_getContext(mCodecCtx->width, mCodecCtx->height, AV_PIX_FMT_YUV420P,
                mCodecCtx->width, mCodecCtx->height, AV_PIX_FMT_BGR24, SWS_POINT, NULL, NULL, NULL);
        }

        if (mFrameRGB->data[0] == NULL)
        {
            mat.create(cvSize(mCodecCtx->width, mCodecCtx->height), CV_8UC3);
            avpicture_fill((AVPicture*)mFrameRGB, (uint8_t*)mat.ptr<char>(0), AV_PIX_FMT_RGB24, 640, 480);
        }
        sws_scale(mImgConvertCtx, mFrameSrc->data, mFrameSrc->linesize, 0, 480, mFrameRGB->data, mFrameRGB->linesize);

        cv::imshow("dddd", mat);
        cv::waitKey(1);

        //SDL---------------------------
        //if (NULL == mSdlTexture)
        //{
        //    mSdlTexture = SDL_CreateTexture(mSdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, mCodecCtx->width, mCodecCtx->height);
        //}
        //SDL_UpdateTexture(mSdlTexture, NULL, mFrameYUV->data[0], mFrameYUV->linesize[0]);
        //SDL_RenderClear(mSdlRenderer);
        //// 如果要显示多个视频流，就要指定不同的sdlTexture以及相应的显示位置
        ////SDL_RenderCopy( sdlRenderer, sdlTexture, &sdlRect, &sdlRect );  
        //SDL_RenderCopy(mSdlRenderer, mSdlTexture, NULL, NULL);
        //SDL_RenderPresent(mSdlRenderer);
        ////SDL End-----------------------
        //TRACE("Decode 1 frame\n");
    }
}

int main()
{
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA data;
    if (WSAStartup(sockVersion, &data) != 0)
    {
        return 0;
    }

    SOCKET sclient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sclient == INVALID_SOCKET)
    {
        printf("invalid socket !\n");
        return 0;
    }

    sockaddr_in serAddr;
    serAddr.sin_family = AF_INET;
    serAddr.sin_port = htons(6666);
    serAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //服务器的IP地址，可以是：①连接外网后分配的②手动设置的  
    if (connect(sclient, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR)
    {
        printf("connect error !\n");
        closesocket(sclient);
        return 0;
    }


    initFfmpeg();


    int iZeroCount = 0;
    int idx = 0;
    char buf[1000];        // 接收缓冲区
    char buf2[60000];      // 存放已经解析出的一帧h264数据
    int len = 0;
    while (1)
    {
        int dwBufLen = recv(sclient, buf, 1000, 0);

        for (int i = 0; i < dwBufLen; i++)
        {
            buf2[idx++] = buf[i];
            if (buf[i] == 0)
            {
                iZeroCount++;
            }
            else
            {
                if (iZeroCount >= 3 && buf[i] == 0x01)
                {
                    *((int*)buf2) = 0x01000000;
                    decodeFfmpeg(buf2, idx - 4);
                    idx = 4;
                }
                iZeroCount = 0;
            }
        }
    }

    closesocket(sclient);
    WSACleanup();
}