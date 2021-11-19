#include "MainForm.h"
#include "ui_MainForm.h"

#include <iostream>

extern "C"
{
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
}

MainForm::MainForm(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::MainForm)
{
	ui->setupUi(this);	
}

MainForm::~MainForm()
{
	delete ui;
}

void MainForm::on_start_clicked()
{
	m_bexit_record = false;
	m_input_fmt_ctx = avformat_alloc_context();
	int ret = avformat_open_input(&m_input_fmt_ctx, ui->lineEdit->text().toStdString().c_str(), NULL, NULL);
	if (ret < 0)
	{
		return;
	}

	ret = avformat_find_stream_info(m_input_fmt_ctx, NULL);
	if (ret < 0)
	{
		fprintf(stderr, "Could not find stream information\n");
		return;
	}

	m_output_filename = ui->outputname->text() + "." + ui->comboBox->currentText();
	ret = avformat_alloc_output_context2(&m_output_fmt_ctx, NULL, ui->comboBox->currentText().toStdString().c_str(), m_output_filename.toStdString().c_str());
	if (ret < 0)
	{
		return;
	}
	int video_stream_index = -1;
	int audio_stream_index = -1;
	
	//add stream
	for (int i=0; i<m_input_fmt_ctx->nb_streams; i++)
	{
		if (m_input_fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			AVStream* video_stream = avformat_new_stream(m_output_fmt_ctx, m_input_fmt_ctx->streams[i]->codec->codec);
			if (!video_stream)
			{
				std::cout << "new video stream error" << std::endl;
				return;
			}
			video_stream_index = video_stream->index;


			if (avcodec_copy_context(video_stream->codec, m_input_fmt_ctx->streams[i]->codec) < 0)
			{
				std::cout << "copy codec settings to output stream error" << std::endl;
			}
		}
		else if (m_input_fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			AVStream* audio_stream = avformat_new_stream(m_output_fmt_ctx, m_input_fmt_ctx->streams[i]->codec->codec);
			if (!audio_stream)
			{
				std::cout << "new audio stream error" << std::endl;
				return;
			}
			audio_stream_index = audio_stream->index;

			if (avcodec_copy_context(audio_stream->codec, m_input_fmt_ctx->streams[i]->codec) < 0)
			{
				std::cout << "copy codec settings to output stream error" << std::endl;
			}
		}

	}

	/* open the output file, if needed */    
	if (!(m_output_fmt_ctx->flags & AVFMT_NOFILE)) 
	{ 
		ret = avio_open(&m_output_fmt_ctx->pb, m_output_filename.toStdString().c_str(), AVIO_FLAG_WRITE);       
		if (ret < 0) 
		{ 
			fprintf(stderr, "Could not open '%s': %d\n", m_output_filename.toStdString().c_str(), ret);
			return; 
		} 
	}

	//写mp4头
	ret = avformat_write_header(m_output_fmt_ctx, nullptr);
	if (ret < 0) {
		("[ABox][MP] write header failed:%d filename:%s", ret, m_output_fmt_ctx->url);
		return ;
	}

   //start 录制mp4线程，进行写Body操作
	m_write_mp4_thread = std::shared_ptr<std::thread>(new std::thread([&,audio_stream_index,video_stream_index]() {
		AVPacket* packet = nullptr;
		packet = av_packet_alloc();
		packet->data = nullptr;
		packet->size = 0;
		av_init_packet(packet);
		while (!m_bexit_record)
		{
			auto ret = av_read_frame(m_input_fmt_ctx, packet);
			if (ret < 0)
			{
				break;
			}

			if (packet->stream_index == audio_stream_index)
			{
				std::cout << "audio packet pts: " << packet->pts << " dts:" << packet->dts << std::endl;
			}else if (packet->stream_index == video_stream_index)
			{
				std::cout << "video packet pts: " << packet->pts << " dts:" << packet->dts << std::endl;
			}
			else {
				continue;
			}
			
			ret = av_interleaved_write_frame(m_output_fmt_ctx, packet);
			if (ret < 0)
			{
				std::cout << "av_interleaved_write_frame error:" << ret << std::endl;
			}
		}

		}), [&](std::thread* p) {
			m_bexit_record = true;
			if (p->joinable())
			{
				p->join();
			}
		}
	);
	
}
void MainForm::on_stop_clicked()
{
	//停止写mp4线程
	m_write_mp4_thread.reset();
	
	//写MP4录制尾巴
	av_write_trailer(m_output_fmt_ctx);
	if (m_output_fmt_ctx && !(m_output_fmt_ctx->flags & AVFMT_NOFILE))
		avio_close(m_output_fmt_ctx->pb);
	avformat_free_context(m_output_fmt_ctx);
	avformat_close_input(&m_input_fmt_ctx);
}
