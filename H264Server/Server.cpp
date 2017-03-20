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

#include <stdio.h>  
#include <winsock2.h>  
#pragma comment(lib,"ws2_32.lib")  

unsigned int WorkerThread(IN LPVOID pParam, SOCKET s)
{

    // TODO: 在此添加控件通知处理程序代码
    cv::VideoCapture vc(0);
    cv::Mat mat;
    char * rgb_buf = NULL;
    int nWidth = 640;
    int nHeight = 480;
    AVFrame *m_pRGBFrame = new AVFrame[1];  //RGB帧数据
    AVFrame *m_pYUVFrame = new AVFrame[1];;  //YUV帧数据
    memset(m_pRGBFrame, 0, sizeof(AVFrame));
    memset(m_pYUVFrame, 0, sizeof(AVFrame));

    av_register_all();
    avcodec_register_all();

    AVCodec *pCodecH264 = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!pCodecH264)
    {
        fprintf(stderr, "h264 codec not found\n");
        exit(1);
    }
    AVCodecContext *pCodecH264Ctx = avcodec_alloc_context3(pCodecH264);
    pCodecH264Ctx->bit_rate = 3000000;// put sample parameters
    pCodecH264Ctx->width = nWidth;//
    pCodecH264Ctx->height = nHeight;//
                                    // frames per second
    AVRational rate;
    rate.num = 1;
    rate.den = 25;
    pCodecH264Ctx->time_base = rate;//(AVRational){1,25};
    pCodecH264Ctx->gop_size = 10; // emit one intra frame every ten frames
    pCodecH264Ctx->max_b_frames = 1;
    pCodecH264Ctx->thread_count = 1;
    pCodecH264Ctx->pix_fmt = AV_PIX_FMT_YUV420P;//PIX_FMT_RGB24;

                                                //av_opt_set(pCodecH264Ctx->priv_data, "preset" "slow", 0);
                                                //av_opt_set(pCodecH264Ctx->priv_data, "libvpx-1080p.ffpreset", NULL, 0);
                                                //打开编码器
    if (avcodec_open2(pCodecH264Ctx, pCodecH264, NULL) < 0)
        printf("不能打开编码库");

    int size = pCodecH264Ctx->width * pCodecH264Ctx->height;
    char * yuv_buff = new char[(size * 3) / 2]; // size for YUV 420

    AVPacket avpkt;
    //图象编码
    int outbuf_size = 100000;
    char * outbuf = new char[outbuf_size];
    int u_size = 0;

    //初始化SwsContext  AV_PIX_FMT_BGR24
    SwsContext * scxt = sws_getContext(pCodecH264Ctx->width, pCodecH264Ctx->height, AV_PIX_FMT_BGR24, \
        pCodecH264Ctx->width, pCodecH264Ctx->height, AV_PIX_FMT_YUV420P, SWS_POINT, NULL, NULL, NULL);

    int iRecordCount = 0;
    while (1)
    {
        vc >> mat;  // 等等记得看下每次>>mat后，rgb_buf是否有变化,如果没变化，则avpicture_fill只需调用1次即可.    
                    // 预览窗口

        if (NULL == m_pRGBFrame->data[0])
        {
            rgb_buf = mat.ptr<char>(0);
            // AV_PIX_FMT_RGB24
            avpicture_fill((AVPicture*)m_pRGBFrame, (uint8_t*)rgb_buf, AV_PIX_FMT_RGB24, nWidth, nHeight);
        }
        if (NULL == m_pYUVFrame->data[0])
        {
            //将YUV buffer 填充YUV Frame
            avpicture_fill((AVPicture*)m_pYUVFrame, (uint8_t*)yuv_buff, AV_PIX_FMT_YUV420P, nWidth, nHeight);
        }
        //将RGB转化为YUV
        sws_scale(scxt, m_pRGBFrame->data, m_pRGBFrame->linesize, 0, pCodecH264Ctx->height, m_pYUVFrame->data, m_pYUVFrame->linesize);

        int got_packet_ptr = 0;
        av_init_packet(&avpkt);
        avpkt.data = (uint8_t *)outbuf;
        avpkt.size = outbuf_size;
        u_size = avcodec_encode_video2(pCodecH264Ctx, &avpkt, m_pYUVFrame, &got_packet_ptr);
        m_pYUVFrame->pts++;     // 很重要，没有这句，视频将越来越模糊
        if (u_size == 0 && avpkt.size > 0)
        {
            //fwrite(avpkt.data, 1, avpkt.size, outputFile);
            int iCommit = 0;
            while (iCommit < avpkt.size)
            {
                iCommit += send(s, (char*)avpkt.data + iCommit, avpkt.size - iCommit, 0);
            }
        }

        cv::waitKey(40);
        cv::imshow("11", mat);
    }
    delete[]m_pRGBFrame;
    delete[]m_pYUVFrame;
    delete[] outbuf;
    avcodec_close(pCodecH264Ctx);
    av_free(pCodecH264Ctx);
    vc.release();
}

int main(void)
{
    //初始化WSA  
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0)
    {
        return 0;
    }
    //创建套接字  
    SOCKET slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (slisten == INVALID_SOCKET)
    {
        printf("socket error !");
        return 0;
    }
    //绑定IP和端口  
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(6666);//端口8888  
    sin.sin_addr.S_un.S_addr = INADDR_ANY;
    if (bind(slisten, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
    {
        printf("bind error !");
    }

    //开始监听  
    if (listen(slisten, 5) == SOCKET_ERROR)
    {
        printf("listen error !");
        return 0;
    }

    //循环接收数据  
    SOCKET sClient;
    sockaddr_in remoteAddr;
    int nAddrlen = sizeof(remoteAddr);

    printf("等待连接...\n");
    do
    {
        sClient = accept(slisten, (SOCKADDR *)&remoteAddr, &nAddrlen);
    } while (sClient == INVALID_SOCKET);
    printf("接受到一个连接：%s \r\n", inet_ntoa(remoteAddr.sin_addr));

    while (true)
    {
        WorkerThread(NULL, sClient);
    }
    closesocket(slisten);
    WSACleanup();
    return 0;
}


//-----------------------------------------------------------------------------------------------
/**
* @file
* API example for demuxing, decoding, filtering, encoding and muxing
* @example transcoding.c
*本例子实现从摄像头获取数据并保存到usb.ts文件中.
*/
/*
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#define inline __inline

#pragma warning(disable:4244)

#include "libavcodec/avcodec.h"
//#include "libavcodec/audioconvert.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/mem.h"
#include "libswscale/swscale.h"
#include "libavresample/avresample.h"
#include "libavdevice/avdevice.h"
#include "libavfilter/avfiltergraph.h"



#pragma warning(default:4244)


//#include <libavcodec/avcodec.h>
//#include <libavformat/avformat.h>
//#include <libavfilter/avfiltergraph.h>
//#include <libavfilter/avcodec.h>
//#include <libavfilter/buffersink.h>
//#include <libavfilter/buffersrc.h>
//#include <libavutil/opt.h>
//#include <libavutil/pixdesc.h>

static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;
typedef struct FilteringContext {
AVFilterContext *buffersink_ctx;
AVFilterContext *buffersrc_ctx;
AVFilterGraph *filter_graph;
} FilteringContext;
static FilteringContext *filter_ctx;

static int open_input_file(const char *filename)
{
int ret;
unsigned int i;
int open_count = 0;
AVInputFormat *file_iformat = NULL;
//打开摄像头
file_iformat = av_find_input_format("vfwcap");
if (!file_iformat)
{
return -1;
}

ifmt_ctx = NULL;

while (1)
{
if ((ret = avformat_open_input(&ifmt_ctx, filename, file_iformat, NULL)) < 0) {
av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
if (open_count++ < 10)
continue;
else
return ret;//十次打开都失败了则失败了
}
break;
}



if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
return ret;
}

for (i = 0; i < ifmt_ctx->nb_streams; i++) {
AVStream *stream;
AVCodecContext *codec_ctx;
stream = ifmt_ctx->streams[i];
codec_ctx = stream->codec;
// Reencode video & audio and remux subtitles etc.
if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
|| codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
// Open decoder
ret = avcodec_open2(codec_ctx,
avcodec_find_decoder(codec_ctx->codec_id), NULL);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
return ret;
}
}
}

av_dump_format(ifmt_ctx, 0, filename, 0);
return 0;
}

static int open_output_file(const char *filename)
{
AVStream *out_stream;
AVStream *in_stream;
AVCodecContext *dec_ctx, *enc_ctx;
AVCodec *encoder;
int ret;
unsigned int i;

ofmt_ctx = NULL;
avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
if (!ofmt_ctx) {
av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
return AVERROR_UNKNOWN;
}


for (i = 0; i < ifmt_ctx->nb_streams; i++) {
out_stream = avformat_new_stream(ofmt_ctx, NULL);
if (!out_stream) {
av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
return AVERROR_UNKNOWN;
}

in_stream = ifmt_ctx->streams[i];
dec_ctx = in_stream->codec;
enc_ctx = out_stream->codec;

if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
|| dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
// 在这个例子中选择H264为编码器
encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
if (!encoder) {
av_log(NULL, AV_LOG_FATAL, "Neccessary encoder not found\n");
return AVERROR_INVALIDDATA;
}

//* In this example, we transcode to same properties (picture size,
//* sample rate etc.). These properties can be changed for output
//* streams easily using filters
if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
enc_ctx->height = dec_ctx->height;
enc_ctx->width = dec_ctx->width;
enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
// take first format from list of supported formats
// enc_ctx->pix_fmt = encoder->pix_fmts[0];
enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
// video time_base can be set to whatever is handy and supported by encoder
enc_ctx->time_base = dec_ctx->time_base;

enc_ctx->me_range = 32;
enc_ctx->scenechange_threshold == 40;
enc_ctx->keyint_min = 15;
enc_ctx->refs = 4;
enc_ctx->qmin = 1;
enc_ctx->qmax = 51;
enc_ctx->max_qdiff = 4;
enc_ctx->bit_rate = 1000000;
enc_ctx->rc_max_rate = 2000000;
enc_ctx->rc_min_rate = 1000000;
enc_ctx->gop_size = 250;
enc_ctx->max_b_frames = 16;
enc_ctx->me_method = 7;
enc_ctx->profile = FF_PROFILE_H264_HIGH;
enc_ctx->codec_descriptor = "cai zi";
enc_ctx->thread_count = 4;
enc_ctx->thread_type = FF_THREAD_FRAME;
enc_ctx->coder_type = FF_CODER_TYPE_AC;//cabac
enc_ctx->b_quant_factor = 1.4;
}
else {
enc_ctx->sample_rate = dec_ctx->sample_rate;
enc_ctx->channel_layout = dec_ctx->channel_layout;
enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
// take first format from list of supported formats
enc_ctx->sample_fmt = encoder->sample_fmts[0];
enc_ctx->time_base = (AVRational) { 1, enc_ctx->sample_rate };
}

// Third parameter can be used to pass settings to encoder
ret = avcodec_open2(enc_ctx, encoder, NULL);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
return ret;
}
}
else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
return AVERROR_INVALIDDATA;
}
else {
// if this stream must be remuxed
ret = avcodec_copy_context(ofmt_ctx->streams[i]->codec,
ifmt_ctx->streams[i]->codec);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Copying stream context failed\n");
return ret;
}
}

if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

}
av_dump_format(ofmt_ctx, 0, filename, 1);

if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
return ret;
}
}

// init muxer, write output file header
ret = avformat_write_header(ofmt_ctx, NULL);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
return ret;
}

return 0;
}

static int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
AVCodecContext *enc_ctx, const char *filter_spec)
{
char args[512];
int ret = 0;
AVFilter *buffersrc = NULL;
AVFilter *buffersink = NULL;
AVFilterContext *buffersrc_ctx = NULL;
AVFilterContext *buffersink_ctx = NULL;
AVFilterInOut *outputs = avfilter_inout_alloc();
AVFilterInOut *inputs = avfilter_inout_alloc();
AVFilterGraph *filter_graph = avfilter_graph_alloc();

if (!outputs || !inputs || !filter_graph) {
ret = AVERROR(ENOMEM);
goto end;
}

if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
buffersrc = avfilter_get_by_name("buffer");
buffersink = avfilter_get_by_name("buffersink");
if (!buffersrc || !buffersink) {
av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
ret = AVERROR_UNKNOWN;
goto end;
}

sprintf(args,
"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
dec_ctx->time_base.num, dec_ctx->time_base.den,
dec_ctx->sample_aspect_ratio.num,
dec_ctx->sample_aspect_ratio.den);

ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
args, NULL, filter_graph);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
goto end;
}

ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
NULL, NULL, filter_graph);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
goto end;
}

ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
(uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
AV_OPT_SEARCH_CHILDREN);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
goto end;
}
}
else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
buffersrc = avfilter_get_by_name("abuffer");
buffersink = avfilter_get_by_name("abuffersink");
if (!buffersrc || !buffersink) {
av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
ret = AVERROR_UNKNOWN;
goto end;
}

if (!dec_ctx->channel_layout)
dec_ctx->channel_layout =
av_get_default_channel_layout(dec_ctx->channels);
sprintf(args,
"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%x",
dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
av_get_sample_fmt_name(dec_ctx->sample_fmt),
dec_ctx->channel_layout);
ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
args, NULL, filter_graph);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
goto end;
}

ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
NULL, NULL, filter_graph);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
goto end;
}

ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
(uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
AV_OPT_SEARCH_CHILDREN);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
goto end;
}

ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
(uint8_t*)&enc_ctx->channel_layout,
sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
goto end;
}

ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
(uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
AV_OPT_SEARCH_CHILDREN);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
goto end;
}
}
else {
ret = AVERROR_UNKNOWN;
goto end;
}

// Endpoints for the filter graph.
outputs->name = av_strdup("in");
outputs->filter_ctx = buffersrc_ctx;
outputs->pad_idx = 0;
outputs->next = NULL;

inputs->name = av_strdup("out");
inputs->filter_ctx = buffersink_ctx;
inputs->pad_idx = 0;
inputs->next = NULL;

if (!outputs->name || !inputs->name) {
ret = AVERROR(ENOMEM);
goto end;
}

if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
&inputs, &outputs, NULL)) < 0)
goto end;

if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
goto end;

//* Fill FilteringContext
fctx->buffersrc_ctx = buffersrc_ctx;
fctx->buffersink_ctx = buffersink_ctx;
fctx->filter_graph = filter_graph;

end:
avfilter_inout_free(&inputs);
avfilter_inout_free(&outputs);

return ret;
}

static int init_filters(void)
{
const char *filter_spec;
unsigned int i;
int ret;
filter_ctx = av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
if (!filter_ctx)
return AVERROR(ENOMEM);

for (i = 0; i < ifmt_ctx->nb_streams; i++) {
filter_ctx[i].buffersrc_ctx = NULL;
filter_ctx[i].buffersink_ctx = NULL;
filter_ctx[i].filter_graph = NULL;
if (!(ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
|| ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO))
continue;


if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
filter_spec = "null"; // passthrough (dummy) filter for video
else
filter_spec = "anull"; // passthrough (dummy) filter for audio
ret = init_filter(&filter_ctx[i], ifmt_ctx->streams[i]->codec,
ofmt_ctx->streams[i]->codec, filter_spec);
if (ret)
return ret;
}
return 0;
}

static int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame) {
int ret;
int got_frame_local;
AVPacket enc_pkt;
int(*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *) =
(ifmt_ctx->streams[stream_index]->codec->codec_type ==
AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;

if (!got_frame)
got_frame = &got_frame_local;

av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
// encode filtered frame
enc_pkt.data = NULL;
enc_pkt.size = 0;
av_init_packet(&enc_pkt);
ret = enc_func(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
filt_frame, got_frame);
av_frame_free(&filt_frame);
if (ret < 0)
return ret;
if (!(*got_frame))
return 0;

// prepare packet for muxing
enc_pkt.stream_index = stream_index;
av_packet_rescale_ts(&enc_pkt,
ofmt_ctx->streams[stream_index]->codec->time_base,
ofmt_ctx->streams[stream_index]->time_base);

av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
// mux encoded frame 把此帧写到ts文件中
ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
return ret;
}

static int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index)
{
int ret;
AVFrame *filt_frame;

av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");
// push the decoded frame into the filtergraph
ret = av_buffersrc_add_frame_flags(filter_ctx[stream_index].buffersrc_ctx,
frame, 0);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
return ret;
}

// pull filtered frames from the filtergraph
while (1) {
filt_frame = av_frame_alloc();
if (!filt_frame) {
ret = AVERROR(ENOMEM);
break;
}
av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
ret = av_buffersink_get_frame(filter_ctx[stream_index].buffersink_ctx,
filt_frame);
if (ret < 0) {
//* if no more frames for output - returns AVERROR(EAGAIN)
//* if flushed and no more frames for output - returns AVERROR_EOF
//* rewrite retcode to 0 to show it as normal procedure completion

if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
ret = 0;
av_frame_free(&filt_frame);
break;
}

filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
ret = encode_write_frame(filt_frame, stream_index, NULL);
if (ret < 0)
break;
}

return ret;
}

static int flush_encoder(unsigned int stream_index)
{
int ret;
int got_frame;

if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
CODEC_CAP_DELAY))
return 0;

while (1) {
av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
ret = encode_write_frame(NULL, stream_index, &got_frame);
if (ret < 0)
break;
if (!got_frame)
return 0;
}
return ret;
}

int main(int argc, char **argv)
{
int ret;
AVPacket packet;
AVFrame *frame = NULL;
enum AVMediaType type;
unsigned int stream_index;
unsigned int i;
int got_frame;
int(*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);


av_register_all();
avfilter_register_all();
//	avformat_network_init();
avcodec_register_all();
avdevice_register_all();

packet.data = NULL;
packet.size = 0;

//这里打开的是USB摄像头,所以传NULL
if ((ret = open_input_file(NULL)) < 0)
goto end;
if ((ret = open_output_file("usb.ts")) < 0)
goto end;
if ((ret = init_filters()) < 0)
goto end;

// read all packets
while (1) {
if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
break;
stream_index = packet.stream_index;
type = ifmt_ctx->streams[packet.stream_index]->codec->codec_type;
av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
stream_index);

if (filter_ctx[stream_index].filter_graph) {
av_log(NULL, AV_LOG_DEBUG, "Going to reencode&filter the frame\n");
frame = av_frame_alloc();
if (!frame) {
ret = AVERROR(ENOMEM);
break;
}
av_packet_rescale_ts(&packet,
ifmt_ctx->streams[stream_index]->time_base,
ifmt_ctx->streams[stream_index]->codec->time_base);
dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 :
avcodec_decode_audio4;
ret = dec_func(ifmt_ctx->streams[stream_index]->codec, frame,
&got_frame, &packet);
if (ret < 0) {
av_frame_free(&frame);
av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
break;
}

if (got_frame) {
frame->pts = av_frame_get_best_effort_timestamp(frame);
av_log(NULL, AV_LOG_ERROR, "pts:%ld\n", frame->pts);
ret = filter_encode_write_frame(frame, stream_index);
av_frame_free(&frame);
if (ret < 0)
goto end;
}
else {
av_frame_free(&frame);
}
}
else {
// remux this frame without reencoding
av_packet_rescale_ts(&packet,
ifmt_ctx->streams[stream_index]->time_base,
ofmt_ctx->streams[stream_index]->time_base);

ret = av_interleaved_write_frame(ofmt_ctx, &packet);
if (ret < 0)
goto end;
}
av_free_packet(&packet);
}

// flush filters and encoders
for (i = 0; i < ifmt_ctx->nb_streams; i++) {
// flush filter
if (!filter_ctx[i].filter_graph)
continue;
ret = filter_encode_write_frame(NULL, i);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
goto end;
}

// flush encoder
ret = flush_encoder(i);
if (ret < 0) {
av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
goto end;
}
}

av_write_trailer(ofmt_ctx);
end:
av_free_packet(&packet);
av_frame_free(&frame);
for (i = 0; i < ifmt_ctx->nb_streams; i++) {
avcodec_close(ifmt_ctx->streams[i]->codec);
if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && ofmt_ctx->streams[i]->codec)
avcodec_close(ofmt_ctx->streams[i]->codec);
if (filter_ctx && filter_ctx[i].filter_graph)
avfilter_graph_free(&filter_ctx[i].filter_graph);
}
av_free(filter_ctx);
avformat_close_input(&ifmt_ctx);
if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
avio_closep(&ofmt_ctx->pb);
avformat_free_context(ofmt_ctx);

if (ret < 0)
av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));

return ret ? 1 : 0;
}


//--------------------------------------------------------------------------------------------------------------------------
/*
#include <libavcodec\avcodec.h>
#include <libavutil\frame.h>
#include <basetsd.h>
#include <libavutil\opt.h>
#include <libavutil\imgutils.h>
#include <libswscale\swscale.h>

int switch_format(AVFrame *pYuvFrame, int nWidth, int nHeight, int nDataLen, char *pData, uint8_t *pYuvBuffer)
{
AVFrame *pRgbFrame = NULL;
pRgbFrame = new AVFrame[1];
SwsContext * scxt = sws_getContext(nWidth, nHeight, AV_PIX_FMT_BGR24, nWidth, nHeight, AV_PIX_FMT_YUV420P, SWS_POINT, NULL, NULL, NULL);


//AVFrame *m_pYUVFrame = new AVFrame[1];
avpicture_fill((AVPicture*)pRgbFrame, (uint8_t*)pData, AV_PIX_FMT_RGB24, nWidth, nHeight);


//将YUV buffer 填充YUV Frame
avpicture_fill((AVPicture*)pYuvFrame, (uint8_t*)pYuvBuffer, AV_PIX_FMT_YUV420P, nWidth, nHeight);


// 翻转RGB图像
// pRgbFrame->data[0]  += pRgbFrame->linesize[0] * (nHeight - 1);
// pRgbFrame->linesize[0] *= -1;
// pRgbFrame->data[1]  += pRgbFrame->linesize[1] * (nHeight / 2 - 1);
// pRgbFrame->linesize[1] *= -1;
// pRgbFrame->data[2]  += pRgbFrame->linesize[2] * (nHeight / 2 - 1);
// pRgbFrame->linesize[2] *= -1;
//将RGB转化为YUV
if (sws_scale(scxt, pRgbFrame->data, pRgbFrame->linesize, 0, nHeight, pYuvFrame->data, pYuvFrame->linesize) < 0)
{
printf("Error\n");
}
if (pRgbFrame)
{
delete pRgbFrame;
}
return 0;
}

int encode_frame(AVCodecContext *pAVContext, AVCodec *pCodec, AVFrame *pAvFrame, IplImage *pImg, FILE *pFile, int nPts, int nCliCnt)
{
int ret, got_output = 0;
AVPacket nAvPkt;
static int nSize = 0;
uint8_t * pYuvBuff = NULL;
int size = pImg->width * pImg->height;
pYuvBuff = (uint8_t *)malloc((size * 3) / 2);
switch_format(pAvFrame, pImg->width, pImg->height, pImg->imageSize, pImg->imageData, pYuvBuff);
av_init_packet(&nAvPkt);
nAvPkt.data = NULL;    // packet data will be allocated by the encoder
nAvPkt.size = 0;
pAvFrame->pts = nPts;
ret = avcodec_encode_video2(pAVContext, &nAvPkt, pAvFrame, &got_output);
if (ret < 0)
{
fprintf(stderr, "Error encoding frame\n");
return -1;
}
if (got_output)
{
nSize += nAvPkt.size;
// printf("Write Frame %d to file size %d total %d\n",nPts,nAvPkt.size,nSize);
if (writetofile)
{
fwrite(nAvPkt.data, 1, nAvPkt.size, pFile);
}
else {
for (int i = 0; i < nCliCnt; i++)
{
send_to_remote("10.0.0.5", 60000 + i, nAvPkt.data, nAvPkt.size, nPts);
}
}
av_free_packet(&nAvPkt);
}
if (pYuvBuff)
{
free(pYuvBuff);
}
return 0;
}

int kk()
{
AVCodec *pAvCodec = NULL;
AVCodecContext *pAvCodecContext = NULL;
AVFrame *pAvFrame = NULL;
uint8_t endcode[] = { 0, 0, 1, 0xb7 };

avcodec_register_all();

pAvCodec = avcodec_find_encoder(AV_CODEC_ID_H264);

pAvCodecContext = avcodec_alloc_context3(pAvCodec);

pAvCodecContext->bit_rate = 200000;
//* resolution must be a multiple of two
pAvCodecContext->width = pFrame->width;
pAvCodecContext->height = pFrame->height;
//* frames per second
pAvCodecContext->time_base.num = 1;
pAvCodecContext->time_base.den = 15;
pAvCodecContext->gop_size = 10; //* emit one intra frame every ten frames
pAvCodecContext->max_b_frames = 1;
pAvCodecContext->thread_count = 20;
pAvCodecContext->thread_type = FF_THREAD_FRAME;
pAvCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
av_opt_set(pAvCodecContext->priv_data, "preset", "slow", 0);
//* open it
if (avcodec_open2(pAvCodecContext, pAvCodec, NULL) < 0) {
fprintf(stderr, "Could not open codec\n");
return -1;
}
pAvFrame = avcodec_alloc_frame();
if (!pAvFrame)
{
fprintf(stderr, "Could not allocate video frame\n");
exit(1);
}
pAvFrame->format = pAvCodecContext->pix_fmt;
pAvFrame->width = pAvCodecContext->width;
pAvFrame->height = pAvCodecContext->height;

ret = av_image_alloc(pAvFrame->data, pAvFrame->linesize, pAvCodecContext->width, pAvCodecContext->height,
pAvCodecContext->pix_fmt, pFrame->align);
if (ret < 0) {
fprintf(stderr, "Could not allocate raw picture buffer\n");
return -1;
}

if (encode_frame(pAvCodecContext, pAvCodec, pAvFrame, pFrame, pFd, n, 1) < 0)
{
return -1
}
}
*/
