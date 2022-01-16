#include "sobel.h"
#include <hls_stream.h>
#include <ap_int.h>

using hls::stream;

typedef short int MYPIXEL;
const int BLOCK_SIZE = 64;
#define FIFO_D 64


struct data_pack {
	MYPIXEL x[BLOCK_SIZE];
};

struct block_pack {
	MYPIXEL x[3*(BLOCK_SIZE+2)];
};

/**
 * ȥ�����ڲ��������������, ͬʱ��֤��dst����ķ����ǰ�BLOCK_SIZE�����.
 * @param fifo
 * @param dst
 */
void depadding(stream<data_pack, FIFO_D> &fifo, PIXEL dst[(HEIGHT-2)*(WIDTH-2)]) {

	// ʹ��ram ʵ�ֵ�fifo������, ͨ�����ƶ���ַ��������֧�ֿɱ仯�Ķ�����������
	// fifo_ram �� fifo_ram2 ������ͬ,Ϊ��ͬʱ������ͬ��ַ����
	MYPIXEL fifo_ram[64*BLOCK_SIZE];
	MYPIXEL fifo_ram2[64*BLOCK_SIZE];
#pragma HLS ARRAY_PARTITION variable=fifo_ram dim=1 factor=BLOCK_SIZE cyclic
#pragma HLS ARRAY_PARTITION variable=fifo_ram2 dim=1 factor=BLOCK_SIZE cyclic

	// ram����������
	MYPIXEL ram_out1[BLOCK_SIZE];
	MYPIXEL ram_out2[BLOCK_SIZE];
#pragma HLS ARRAY_PARTITION variable=ram_out1 dim=1 complete
#pragma HLS ARRAY_PARTITION variable=ram_out2 dim=1 complete

	//
	MYPIXEL depadding_regs[BLOCK_SIZE];
	MYPIXEL depadding_regs1[BLOCK_SIZE];
#pragma HLS ARRAY_PARTITION variable=depadding_regs dim=1 complete
#pragma HLS ARRAY_PARTITION variable=depadding_regs1 dim=1 complete

	ap_uint<6> fifo_waddr = 0;
	ap_uint<6> fifo_raddr = 0;
	data_pack fifo_out;
#pragma HLS ARRAY_PARTITION variable=fifo_out.x dim=1 complete

	int row = 0; // ��Ǵ�fifo_ram�������������ڴ�paddingͼ��ĵڼ���
	int col = 0; // ��Ǵ�fifo_ram�������������ڴ�paddingͼ��ĵڼ���
	int dst_addr = 0;
	int depadding_dnum = 0; // depadding_regs ��ʣ����Ч������
	int preload_num = 24;

/**
 * fifo --> fifo_ram --> depadding_regs --> depadding_regs1 --> dst
 */
	// ǰ2�����ݰ������㣬�ǲ���Ҫ�ģ�����
	for(int i = 0; i < 2*WIDTH/BLOCK_SIZE; i++)
		fifo.read(fifo_out);


	// Ԥ��ȡһЩ���ݵ�fifo_ram��,
	for(int i = 0; i < preload_num; i++) {
		fifo.read(fifo_out);
		for(int n = 0; n < BLOCK_SIZE; n++) {
		#pragma HLS UNROLL factor=BLOCK_SIZE
			fifo_ram[fifo_waddr*BLOCK_SIZE+n] = fifo_out.x[n];
			fifo_ram2[fifo_waddr*BLOCK_SIZE+n] = fifo_out.x[n];
		}
		fifo_waddr++;
	}


	for(int i = 0; i < (HEIGHT-2)*(WIDTH-2)/BLOCK_SIZE ;i++) {
#pragma HLS PIPELINE ii=1
//		assert(fifo_raddr != fifo_waddr);

		// ���ƶ��������պö��� fifo
		if (i < (HEIGHT-2)*(WIDTH-2)/BLOCK_SIZE-1)
			fifo.read(fifo_out);

		// fifo ������д�룬����
		for(int n = 0; n < BLOCK_SIZE; n++) {
		#pragma HLS UNROLL factor=BLOCK_SIZE
			ram_out1[n] = fifo_ram[fifo_raddr*BLOCK_SIZE+n];
			ram_out2[n] = fifo_ram2[(fifo_raddr+1)*BLOCK_SIZE+n];
			fifo_ram[fifo_waddr*BLOCK_SIZE+n] = fifo_out.x[n];
			fifo_ram2[fifo_waddr*BLOCK_SIZE+n] = fifo_out.x[n];
		}
		fifo_waddr++;

		// update depadding_regs and depadding_regs1
		// ���ڵ�0�е�����(col == 0), ����padding�ģ���Ҫȥ��
		for(int n = 0; n < BLOCK_SIZE; n++){
		#pragma HLS UNROLL factor=BLOCK_SIZE
			if(depadding_dnum == 0) {
				if(col != 0) {
					depadding_regs1[n] = ram_out1[n];
					if( col != BLOCK_SIZE-1)
						depadding_regs[n] = ram_out2[n];
					else if(n < BLOCK_SIZE-2)
						depadding_regs[n] = ram_out2[n+2];
				}
				else {
					if(n < BLOCK_SIZE-2) {
						depadding_regs1[n] = ram_out1[n+2];
						depadding_regs[n] = ram_out2[n+2];
					}
					else
						depadding_regs1[n] = ram_out2[n+2-BLOCK_SIZE];

				}
			}
			else {
				if(n < depadding_dnum) {
					depadding_regs1[n] = depadding_regs[n];
					if (col == 0)
						depadding_regs[n] = ram_out1[BLOCK_SIZE-depadding_dnum+2+n];
					else
						depadding_regs[n] = ram_out1[BLOCK_SIZE-depadding_dnum+n];
				}
				else if(col == 0)
					depadding_regs1[n] = ram_out1[n-depadding_dnum+2];
				else
					depadding_regs1[n] = ram_out1[n-depadding_dnum];


			}
		}

		// ����ַ���뷽ʽдdst����
		for(int n = 0; n < BLOCK_SIZE; n++){
		#pragma HLS UNROLL factor=BLOCK_SIZE
			dst[dst_addr + n] = depadding_regs1[n];
		}
		dst_addr += BLOCK_SIZE;


		// ���¶���ַ����depadding_reg��û������ʱ���ɶ���2��
		if(depadding_dnum==0) {
			if (col == 0 || col == WIDTH/BLOCK_SIZE-1)
				depadding_dnum = BLOCK_SIZE - 2;
			else
				depadding_dnum = BLOCK_SIZE;

			col+=2;
			fifo_raddr += 2;

		}
		else {
			if (col == 0)
				depadding_dnum -= 2;
			else
				depadding_dnum = depadding_dnum;
			col+=1;
			fifo_raddr += 1;
		}


		if(col >= WIDTH/BLOCK_SIZE) {
			col = col-WIDTH/BLOCK_SIZE;
			row++;
		}

	}

	// д�� ʣ������  mod(718*1278, 64)=44
	for(int n = 0; n < 44; n++) {
	#pragma HLS UNROLL
		dst[dst_addr+n] = depadding_regs[n];
	}

}



void grad_merge(stream<data_pack, 4> &gradx_fifo, stream<data_pack, 4> &grady_fifo, stream<data_pack, FIFO_D> &fifo)
{
	int block_num_x = WIDTH/BLOCK_SIZE;
	data_pack gradx_out;
	data_pack grady_out;
#pragma HLS ARRAY_PARTITION variable=gradx_out dim=1 complete
#pragma HLS ARRAY_PARTITION variable=grady_out dim=1 complete

	data_pack fifo_in;
#pragma HLS ARRAY_PARTITION variable=fifo_in.x dim=1 complete

	for(int i=0; i < block_num_x*HEIGHT; i++) {
	#pragma HLS PIPELINE ii=1

		gradx_out = gradx_fifo.read();
		grady_out = grady_fifo.read();

		for(int n = 0; n < BLOCK_SIZE; n++) {
#pragma HLS UNROLL
			PIXEL tmpx, tmpy;
			if(gradx_out.x[n] < 0)
				tmpx = 0;
			else if(gradx_out.x[n] > 255)
				tmpx = 255;
			else
				tmpx = gradx_out.x[n];

			if(grady_out.x[n] < 0)
				tmpy = 0;
			else if(grady_out.x[n] > 255)
				tmpy = 255;
			else
				tmpy = grady_out.x[n];

			PIXEL sum = tmpx + tmpy;

			if(sum > 255)
				fifo_in.x[n] = 255;
			else
				fifo_in.x[n] = sum;

		}

		fifo.write(fifo_in);

	}
}

void grad_filter(stream<block_pack, 4> &block_fifo, stream<data_pack, 4> &gradx_fifo, stream<data_pack, 4> &grady_fifo) {
	int block_num_x = WIDTH/BLOCK_SIZE;

	// ͼ���
	MYPIXEL block[3][BLOCK_SIZE+2] = {0};
#pragma HLS ARRAY_PARTITION variable=block dim=2 complete
#pragma HLS ARRAY_PARTITION variable=block dim=1 complete

	data_pack gradx_in;
	data_pack grady_in;
#pragma HLS ARRAY_PARTITION variable=gradx_in.x dim=1 complete
#pragma HLS ARRAY_PARTITION variable=grady_in.x dim=1 complete

	for(int i=0; i < block_num_x*HEIGHT; i++) {
#pragma HLS PIPELINE ii=1

		// ��fifo����block
		block_pack block_fifo_out = block_fifo.read();

		for(int n = 0; n < BLOCK_SIZE+2; n++) {
#pragma HLS UNROLL
			block[0][n] = block_fifo_out.x[n];
			block[1][n] = block_fifo_out.x[BLOCK_SIZE+2+n];
			block[2][n] = block_fifo_out.x[2*(BLOCK_SIZE+2)+n];
		}

		// x �Ტ���˲�
		for(int n = 0; n < BLOCK_SIZE; n++) {
		#pragma HLS UNROLL
		PIXEL tmp1 = block[0][n+2]-block[0][n];
		PIXEL tmp2 = (block[1][n] - block[1][n+2])<<1;
		PIXEL tmp3 = block[2][n] - block[2][n+2];
		PIXEL tmp4 = tmp1 - tmp2;
		PIXEL tmp5 = tmp4 - tmp3;

		gradx_in.x[n] = tmp5;

		}

		// y �Ტ���˲�
		for(int n = 0; n < BLOCK_SIZE; n++) {
		#pragma HLS UNROLL
		PIXEL tmp1 = -block[0][n] + block[2][n];
		PIXEL tmp2 = (block[0][n+1] - block[2][n+1])<<1;
		PIXEL tmp3 = block[0][n+2] - block[2][n+2];
		PIXEL tmp4 = tmp1 - tmp2;
		PIXEL tmp5 = tmp4 - tmp3;

		grady_in.x[n] = tmp5;

		}

		// ���ݶ�д��fifo
		gradx_fifo.write(gradx_in);
		grady_fifo.write(grady_in);

	}
}

void read_block(PIXEL src[HEIGHT*WIDTH], stream<block_pack, 4> &fifo)
{

	// x �����Ŀ
	int block_num_x = WIDTH/BLOCK_SIZE;


	// �������ͼ���
	MYPIXEL block[3][BLOCK_SIZE+2] = {0};
#pragma HLS ARRAY_PARTITION variable=block dim=2 complete
#pragma HLS ARRAY_PARTITION variable=block dim=1 complete

	// �¶������
	MYPIXEL line[BLOCK_SIZE];
#pragma HLS ARRAY_PARTITION variable=line dim=1 complete

	// ǰ���еĻ���
	MYPIXEL line_ram[2][WIDTH];
#pragma HLS ARRAY_RESHAPE variable=line_ram factor=32 dim=2 cyclic
#pragma HLS ARRAY_PARTITION variable=line_ram dim=1 complete

	block_pack out_buffer;
#pragma HLS ARRAY_PARTITION variable=out_buffer.x complete


	int heigh_cnt = 0;
	int width_cnt = 0;

	ILOOP:
	for (int i=0; i < block_num_x*HEIGHT; i++) {
#pragma HLS PIPELINE ii=1
		// ���ⲿ����һС������
		for(int n=0; n < BLOCK_SIZE; n++)
		#pragma HLS UNROLL factor=BLOCK_SIZE
			line[n] = src[i*BLOCK_SIZE + n];

		// ��ǰ���ǰ2����Ҫ����һ��ĺ�2�в���
		block[0][0] = block[0][BLOCK_SIZE]; block[0][1] = block[0][BLOCK_SIZE+1];
		block[1][0] = block[1][BLOCK_SIZE]; block[1][1] = block[1][BLOCK_SIZE+1];
		block[2][0] = block[2][BLOCK_SIZE]; block[2][1] = block[2][BLOCK_SIZE+1];

		//
		for(int n=2; n < BLOCK_SIZE+2; n++) {
		#pragma HLS UNROLL
			block[0][n] = line_ram[0][width_cnt*BLOCK_SIZE + n - 2];
			block[1][n] = line_ram[1][width_cnt*BLOCK_SIZE + n - 2];
			block[2][n] = line[n - 2];

			// write back ram
			line_ram[0][width_cnt*BLOCK_SIZE + n - 2] = block[1][n];
			line_ram[1][width_cnt*BLOCK_SIZE + n - 2] = block[2][n];
		}

		// ��blockд��fifo
		for(int l = 0; l < 3; l++) {
#pragma HLS UNROLL
			for(int n = 0; n < BLOCK_SIZE+2; n++){
#pragma HLS UNROLL
				out_buffer.x[l*(BLOCK_SIZE+2)+n] = block[l][n];
			}
		}

		fifo.write(out_buffer);


		// ����������
		if (width_cnt == WIDTH/BLOCK_SIZE-1) {
			width_cnt = 0;
			heigh_cnt++;
		}
		else {
			width_cnt++;
		}
	}



}

void sobel(PIXEL src[HEIGHT*WIDTH], PIXEL dst[(HEIGHT-2)*(WIDTH-2)], int rows, int cols)
{
// Ĭ���ۺ�Ϊ˫�˿�ram����˻������ӿ���Сһ��
#pragma HLS ARRAY_PARTITION variable=src dim=1 factor=BLOCK_SIZE/2 cyclic
#pragma HLS ARRAY_PARTITION variable=dst dim=1 factor=BLOCK_SIZE/2 cyclic


#pragma HLS DATAFLOW

	stream<block_pack, 4> block_fifo; //
	stream<data_pack, 4> gradx_fifo;
	stream<data_pack, 4> grady_fifo;
	stream<data_pack, FIFO_D> fifo;

	// ���ж�ȡͼ�����3*(BLOCK_SIZE+2)��ͼ���
	read_block(src, block_fifo);
	// ��ͼ���ֱ�����˲����õ�x�� �� y����ݶ�
	grad_filter(block_fifo, gradx_fifo, grady_fifo);
	// �ϲ�����������ݶ�
	grad_merge(gradx_fifo, grady_fifo, fifo);
	// ȥ������Ĳ��֣�
	depadding(fifo, dst);
}
