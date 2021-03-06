#include "MainForm.h"
#include "ui_MainForm.h"

#include <iostream>

extern "C"
{
#include <libavutil/timestamp.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
}

char ts_str[AV_TS_MAX_STRING_SIZE] = { 0 };
#define av_ts2timestr2(ts, tb) av_ts_make_time_string(ts_str, ts, tb)
#define av_ts2str2(ts) av_ts_make_string(ts_str, ts)
#define av_err2str2(errnum) \
    av_make_error_string(ts_str, AV_ERROR_MAX_STRING_SIZE, errnum)

static void log_packet(const AVFormatContext* fmt_ctx, const AVPacket* pkt, const char* tag)
{
	AVRational* time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
	printf("%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
		tag,
		av_ts2str2(pkt->pts), av_ts2timestr2(pkt->pts, time_base),
		av_ts2str2(pkt->dts), av_ts2timestr2(pkt->dts, time_base),
		av_ts2str2(pkt->duration), av_ts2timestr2(pkt->duration, time_base),
		pkt->stream_index);
}

MainForm::MainForm(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::MainForm)
{
	ui->setupUi(this);	
	ui->textBrowser->moveCursor(QTextCursor::End,QTextCursor::KeepAnchor);
}

MainForm::~MainForm()
{
	delete ui;
}

void MainForm::on_start_clicked()
{
	m_bexit_record = false;
	ui->textBrowser->append(QString("[INFO]start remuxing \n"));
	ui->start->setEnabled(false);
	ui->stop->setEnabled(true);
	std::string input_name = ui->lineEdit->text().toStdString();
	const char* input_filename = input_name.c_str();
	int ret = avformat_open_input(&m_input_fmt_ctx, input_filename, NULL, NULL);
	if (ret < 0)
	{
		return;
	}

	ret = avformat_find_stream_info(m_input_fmt_ctx, NULL);
	if (ret < 0)
	{
		ui->textBrowser->append("Could not find stream information\n");
		return;
	}

	av_dump_format(m_input_fmt_ctx, 0, input_filename, 0);

	m_output_filename = ui->outputname->text() + "." + ui->comboBox->currentText();

	ret = avformat_alloc_output_context2(&m_output_fmt_ctx, NULL, NULL, m_output_filename.toStdString().c_str());
	if (ret < 0)
	{
		return;
	}
	int video_stream_index = -1;
	int audio_stream_index = -1;
	
	//add stream
	for (int i=0; i<m_input_fmt_ctx->nb_streams; i++)
	{
		if (m_input_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			AVStream* video_stream = avformat_new_stream(m_output_fmt_ctx, m_input_fmt_ctx->streams[i]->codec->codec);
			if (!video_stream)
			{
				ui->textBrowser->append("[ERRRO]new video stream error\n");
				return;
			}
			video_stream_index = video_stream->index;


			if (avcodec_parameters_copy(video_stream->codecpar, m_input_fmt_ctx->streams[i]->codecpar) < 0)
			{
				ui->textBrowser->append("[ERROR]copy codec settings to output stream error\n");
			}
			video_stream->codecpar->codec_tag = 0;
		}
		else if (m_input_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			AVStream* audio_stream = avformat_new_stream(m_output_fmt_ctx, m_input_fmt_ctx->streams[i]->codec->codec);
			if (!audio_stream)
			{
				ui->textBrowser->append("[ERROR]add new audio stream error\n");
				return;
			}
			audio_stream_index = audio_stream->index;

			if (avcodec_parameters_copy(audio_stream->codecpar, m_input_fmt_ctx->streams[i]->codecpar) < 0)
			{
				ui->textBrowser->append("[ERROR]copy codec settings to output stream error\n");
			}

			audio_stream->codecpar->codec_tag = 0;
		}

	}
	std::string type = ui->comboBox->currentText().toStdString();


	av_dump_format(m_output_fmt_ctx, 0, m_output_filename.toStdString().c_str(), 1);


	/* open the output file, if needed */    
	if (!(m_output_fmt_ctx->oformat->flags & AVFMT_NOFILE)) 
	{ 
		ret = avio_open(&m_output_fmt_ctx->pb, m_output_filename.toStdString().c_str(), AVIO_FLAG_WRITE);       
		if (ret < 0) 
		{ 
			ui->textBrowser->append(QString("Could not open '%1': %2\n").arg(m_output_filename.toStdString().c_str()).arg(ret));
			return; 
		} 
	}


	//写mp4头
	ret = avformat_write_header(m_output_fmt_ctx, nullptr);
	if (ret < 0) {
		ui->textBrowser->append(QString("[ABox][MP] write header failed : %1 filename : %2\n").arg(ret).arg(m_output_fmt_ctx->url));
		return ;
	}

   //start 录制mp4线程，进行写Body操作
	m_write_mp4_thread = std::shared_ptr<std::thread>(new std::thread([this, video_stream_index, audio_stream_index]() {
		AVPacket packet;
		while (!m_bexit_record)
		{
			auto ret = av_read_frame(m_input_fmt_ctx, &packet);
			if (ret < 0)
			{
				break;
			}

			AVStream *input_steam = m_input_fmt_ctx->streams[packet.stream_index];
			if (input_steam->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				packet.stream_index = audio_stream_index;
			}
			else if (input_steam->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				packet.stream_index = video_stream_index;
			}
			else {
				av_packet_unref(&packet);
				continue;
			}
		
			AVRational itime = m_input_fmt_ctx->streams[packet.stream_index]->time_base;
			AVRational otime = m_output_fmt_ctx->streams[packet.stream_index]->time_base;
			packet.pts = av_rescale_q_rnd(packet.pts, itime, otime, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			packet.dts = av_rescale_q_rnd(packet.dts, itime, otime, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			packet.duration = av_rescale_q(packet.duration, itime, otime);
			packet.pos = -1;
			log_packet(m_output_fmt_ctx, &packet, "out");
			ret = av_interleaved_write_frame(m_output_fmt_ctx, &packet);
			static int num = 0;
			if (num++ > 100)
			{
				ui->textBrowser->append(QString("[INFO]av_interleaved_write_frame 100 packet\n"));
				num = 0;
			}
		
			if (ret < 0)
			{
				ui->textBrowser->append(QString("[ERROR]av_interleaved_write_frame error: '%1'\n").arg(ret));
			}
			av_packet_unref(&packet);
		}
		av_write_trailer(m_output_fmt_ctx);
		}), [&](std::thread* p) {
			if (p->joinable())
			{	
				m_bexit_record = true;
				p->join();
			}
		}
	);
}
void MainForm::on_stop_clicked()
{
	//停止写mp4线程
	m_write_mp4_thread.reset();
	ui->textBrowser->append(QString("[INFO]close remuxing \n"));
	//写MP4录制尾巴
	if (m_input_fmt_ctx)
	{
		avformat_close_input(&m_input_fmt_ctx);
		m_input_fmt_ctx = nullptr;
	}

	
	if (m_output_fmt_ctx && !(m_output_fmt_ctx->oformat->flags & AVFMT_NOFILE))
		avio_close(m_output_fmt_ctx->pb);
	if (m_output_fmt_ctx)
	{
		avformat_free_context(m_output_fmt_ctx);
		m_output_fmt_ctx = nullptr;
	}
	ui->start->setEnabled(true);
	ui->stop->setEnabled(false);

}
