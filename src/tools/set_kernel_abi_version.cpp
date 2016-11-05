#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <boot/kernel_args.h>

#define EI_ABIVERSION 0x8

int main(int argc, const char* const* argv)
{
	if (argc < 2)
		return 1;

	const char* fileName = argv[1];

	int fd = open(fileName, O_RDWR);
	if (fd < 0)
		return 2;

	unsigned const int size = EI_ABIVERSION + 1;
	unsigned char ident[size];
	ssize_t bytes = read(fd, ident, size);
	if (bytes != size)
		return 2;

	ident[EI_ABIVERSION] = CURRENT_KERNEL_ARGS_VERSION;
	if (lseek(fd, 0, SEEK_SET) < 0)
		return 2;

	bytes = write(fd, ident, size);
	if (bytes != size)
		return 2;

	close(fd);

	return 0;
}
