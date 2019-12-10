#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <complex>
#include <chrono>
#include <thread> 
#include <string>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <cassert>
#include <png.h>
#include <mutex>

using namespace std;
using namespace chrono;

unsigned UPSCALE = 2;

unsigned WIDTH = 1920;
unsigned HEIGTH = 1080;
constexpr png_byte BIT_DEPTH = 8;
constexpr png_byte COLOR_TYPE = PNG_COLOR_TYPE_RGBA;
unsigned MAX_ITER = 50;

double kinda_zoom = 1.0 / (0.5); // change value in parenthesis
double startx = -0.5 * kinda_zoom / 2.0;
double starty = 0 * kinda_zoom;

double W_FACTOR = (double)WIDTH / HEIGTH;

double MAX_RE = startx + W_FACTOR * kinda_zoom / 2;
double MIN_RE = startx - W_FACTOR * kinda_zoom / 2;
double MAX_IM = starty + kinda_zoom / 2;
double MIN_IM = starty - kinda_zoom / 2;

const char* FILENAME;

png_infop info_ptr;
png_structp png_ptr;
png_bytep* row_ptrs;

FILE* fp;

void create_png()
{
	fp = fopen(FILENAME, "wb");
	assert(fp != nullptr);

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	assert(png_ptr != nullptr);

	info_ptr = png_create_info_struct(png_ptr);
	assert(info_ptr != nullptr);

	png_init_io(png_ptr, fp);

	png_set_IHDR(png_ptr, info_ptr, WIDTH, HEIGTH, BIT_DEPTH, COLOR_TYPE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png_ptr, info_ptr);

	row_ptrs = (png_bytep*)malloc(sizeof(png_bytep) * HEIGTH);
	if (row_ptrs != nullptr)
	{
		for (size_t i = 0; i < HEIGTH; i++)
		{
			row_ptrs[i] = (png_bytep)malloc(sizeof(png_byte) * WIDTH * 4);
		}
	}
}

void write_and_close_png()
{
	png_write_image(png_ptr, row_ptrs);
	png_write_end(png_ptr, info_ptr);
	for (size_t i = 0; i < HEIGTH; i++)
	{
		free(row_ptrs[i]);
	}
	free(row_ptrs);
	fclose(fp);
}

void HSVtoRGB(int H, double S, double V, int output[3]) {
	double C = S * V;
	double X = C * (1 - abs(fmod(H / 60.0, 2) - 1));
	double m = V - C;
	double Rs, Gs, Bs;

	if (H >= 0 && H < 60) {
		Rs = C;
		Gs = X;
		Bs = 0;
	}
	else if (H >= 60 && H < 120) {
		Rs = X;
		Gs = C;
		Bs = 0;
	}
	else if (H >= 120 && H < 180) {
		Rs = 0;
		Gs = C;
		Bs = X;
	}
	else if (H >= 180 && H < 240) {
		Rs = 0;
		Gs = X;
		Bs = C;
	}
	else if (H >= 240 && H < 300) {
		Rs = X;
		Gs = 0;
		Bs = C;
	}
	else {
		Rs = C;
		Gs = 0;
		Bs = X;
	}

	output[0] = (int)((Rs + m) * 255);
	output[1] = (int)((Gs + m) * 255);
	output[2] = (int)((Bs + m) * 255);
}

mutex _lock;

//double i_sum = 0;
void draw_mandelbrot(int y, int to_y)
{
	for (; y < to_y; y++)
	{
		for (size_t x = 0; x < WIDTH; x++)
		{
			complex<double> c;
			c = 1i * (MAX_IM - y * (MAX_IM - MIN_IM) / ((double)HEIGTH - 1));
			c += MIN_RE + x * (MAX_RE - MIN_RE) / ((double)WIDTH - 1);
			size_t i;
			complex<double> z;
			z += c;
			for (i = 0; i < MAX_ITER; i++)
			{
				if (abs(z) > 2.0)
				{
					break;
				}
				z *= z;
				z += c;
			}
			png_byte tmp[3];
			if (i != MAX_ITER)
			{
				//i_sum +=i;
				int hue = (int)(160 + 120 - 120 * pow((double)i / ((double)MAX_ITER - 1), 1.3));
				hue = clamp(hue, 0, 300);
				double val = pow((double)i / ((double)MAX_ITER - 1), 1.3);
				val = clamp(val, 0.0, 1.0);
				int hsvout[3];
				HSVtoRGB(hue, 1.0, val, hsvout);
				for (uint8_t chan = 0; chan < 3; chan++)
				{
					tmp[chan] = (png_byte)hsvout[chan];
				}
			}
			else
			{
				for (uint8_t chan = 0; chan < 3; chan++)
				{
					tmp[chan] = 0;
				}

			}

			_lock.lock();
			for (uint8_t chan = 0; chan < 3; chan++)
			{
				row_ptrs[y][x * 4 + chan] = tmp[chan];
			}

			row_ptrs[y][x * 4 + 3] = 255;
			_lock.unlock();
		}
	}
}

void err_exit(string msg)
{
	if (fp != nullptr)
	{
		write_and_close_png();
	}
	cin.ignore(256, '\n');
	cout << msg << endl << "Press any key to exit..." << endl;
	cin.get();
	exit(1);
}

int main(int argc, char** argv)
{
    if(argc > 1)
    {
        if(argc & 1)
        {
            for (size_t i = 1; i < argc; i += 2)
            {
                if(!strncmp(argv[i], "-W", 2) || !strncmp(argv[i], "--width", 7))
                {
                    int __tmp = stoi(argv[i + 1]);
                    if(__tmp > 0 && __tmp < (1 << 20))
                    {
                        WIDTH = (unsigned)__tmp;
                    }
					else
					{
						err_exit("Width must be int in range (1, 1048575)");
					}
                }
				else if (!strncmp(argv[i], "-H", 2) || !strncmp(argv[i], "--heigth", 8))
				{
					int __tmp = stoi(argv[i + 1]);
                    if(__tmp > 0 && __tmp < (1 << 20))
                    {
                        HEIGTH = (unsigned)__tmp;
                    }
					else
					{
						err_exit("Heigth must be int in range [1, 1048575]");
					}
				}
				else if (!strncmp(argv[i], "-S", 2) || !strncmp(argv[i], "--scale", 7))
				{
					int __tmp = stoi(argv[i + 1]);
                    if(__tmp > 0 && __tmp <= (1 << 6))
                    {
                        UPSCALE = (unsigned)__tmp;
                    }
					else
					{
						err_exit("Upscale must be int in range [1, 64]");
					}
				}
				else if (!strncmp(argv[i], "-I", 2) || !strncmp(argv[i], "--iters", 7))
				{
					int __tmp = stoi(argv[i + 1]);
                    if(__tmp > 0 && __tmp <= (100'000))
                    {
                        MAX_ITER = (unsigned)__tmp;
                    }
					else
					{
						err_exit("Max iterations must be int in range [1, 100000]");
					}
				}
				else if (!strncmp(argv[i], "-Z", 2) || !strncmp(argv[i], "--zoom", 6))
				{
					double __tmp = stod(argv[i + 1]);
					if (__tmp > 0 && __tmp <= 5)
					{
						kinda_zoom = 1 / (__tmp);
					}
					else
					{
						err_exit("Zoom must be float in range (0, 5]");
					}
				}
				else if (!strncmp(argv[i], "-F", 2) || !strncmp(argv[i], "--file", 6))
				{
					filesystem::path p = filesystem::path(argv[i + 1]);
					if (p.has_filename())
					{
						FILENAME = argv[i + 1];
					}
					else
					{
						err_exit("Wrong filename");
					}
				}
				else
				{
					err_exit("Wrong argument");
				}
            }
        }
        else
		{
			err_exit("Invalid argument count. Launch program without parameters to see usage.");
		}
    }
	else
	{
		cout << "Usage: [--width(-W) width] [--heigth(-H) heigth] -- image resolution\n"
				"       [--scale(-S) upscale] -- upscale factor (int)\n"
				"       [--iters(-I) iters]   -- maximum iterations in set calculation\n"
		  	    "       [--zoom(-Z) zoom]     -- plane zoom factor, float\n"
	  	        "       [--file(-F) filename] -- optional filename\n\n";
		cout << "Program launched without parameters, proceed with defaults? y/n" << endl;
		char ans;
		cin >> ans;
		if (ans == 'y')
		{
			cout << endl;
		}
		else if (ans == 'n')
		{
			exit(0);
		}
		else
		{
			err_exit("Wrong char.");
		}
		
	}
	WIDTH *= UPSCALE;
	HEIGTH *= UPSCALE;
	startx = -0.5 * kinda_zoom / 2.0;
	starty = 0 * kinda_zoom;

	W_FACTOR = (double)WIDTH / HEIGTH;

	MAX_RE = startx + W_FACTOR * kinda_zoom / 2;
	MIN_RE = startx - W_FACTOR * kinda_zoom / 2;
	MAX_IM = starty + kinda_zoom / 2;
	MIN_IM = starty - kinda_zoom / 2;

	if (!FILENAME)
	{
		char buffer[256];
		sprintf(buffer, "mandelbrot_%dx_%dx%d_%diters.png", UPSCALE, WIDTH / UPSCALE, HEIGTH / UPSCALE, MAX_ITER);
		FILENAME = buffer;
	}
	
	cout << "Mandelbrot v0.1 by eXCore\n";
	cout << "Resolution(WxH): " << WIDTH / UPSCALE << "x" << HEIGTH / UPSCALE << " upscaled " << UPSCALE << "x\n";
	cout << "Maximum iterations: " << MAX_ITER << "\n";
	cout << "Plane zoom: " << fixed << setprecision(3) << (1.0 / kinda_zoom) << "x\n" << endl;

	high_resolution_clock::time_point t_start = high_resolution_clock::now();

	create_png();

	unsigned max_threads = thread::hardware_concurrency();
	unsigned DIV_INTO = max(1u, HEIGTH * MAX_ITER / 2000 / 50);
	//unsigned max_threads = 1; // for test purposes

	vector<thread> threads(max_threads);

	unsigned prev = 0;
	for (size_t iteration = 0; iteration < DIV_INTO; iteration++)
	{
		for (size_t i = 0; i < max_threads; i++)
		{
			cerr << "Thread launched from " << prev << " up to " << prev + HEIGTH / max_threads / DIV_INTO << endl;
			threads[i] = thread(draw_mandelbrot, prev, prev + HEIGTH / max_threads / DIV_INTO);
			prev += HEIGTH / max_threads / DIV_INTO;
		}
		for (size_t i = 0; i < max_threads; i++)
		{
			threads[i].join();
		}
	}
	if (prev != HEIGTH)
	{
		cerr << "Thread launched from " << prev << " up to " << HEIGTH << endl;
		threads[0] = thread(draw_mandelbrot, prev, HEIGTH);
		threads[0].join();
	}
	high_resolution_clock::time_point t_end = high_resolution_clock::now();

	write_and_close_png();
	cout << "mandelbrot?" << endl;
	duration<double> t_span = duration_cast<duration<double>>(t_end - t_start);
	cout << "process time: " << t_span.count() << "s\n" << endl;
	//cout << "avg i is " << (i_sum) / (WIDTH*HEIGTH) << endl; // needs rewrite to work, considering threads sharing

	cout << "Press any key to exit...\n";
	cin.get();
	return 0;
}
