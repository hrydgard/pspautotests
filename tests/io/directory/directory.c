#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

static int compare (const void * a, const void * b)
{
    /* The pointers point to offsets into "array", so we need to
       dereference them to get at the strings. */
    return strcmp (((struct SceIoDirent*)a)->d_name, ((struct SceIoDirent*)b)->d_name);
}


int main(int argc, char **argv) {
	struct SceIoDirent files[32];
	int fd;
	int cnt = 0;
	int i = 0;

	fd = sceIoDopen("FakeDir");
	printf("%08x\n",fd);

	fd = sceIoDopen("folder");
	while (1) {
		if (sceIoDread(fd, &files[cnt]) <= 0) break;
		cnt++;
	}
	sceIoDclose(fd);
	
	qsort (files, cnt, sizeof (struct SceIoDirent), compare);
	
	for(i = 0; i < cnt; i++)
	{
		printf(
				"'%s': size : %lld\n",
				files[i].d_name,
				files[i].d_stat.st_size
			);
		printf(
				"'%s': attr : %d\n",
				files[i].d_name,
				files[i].d_stat.st_attr
			);
		printf(
				"'%s': mode : %d\n",
				files[i].d_name,
				files[i].d_stat.st_mode
			);
		printf(
				"'%s': creation date : %d-%d-%d-%d-%d-%d-%d\n",
				files[i].d_name,
				files[i].d_stat.st_ctime.year,
				files[i].d_stat.st_ctime.month,
				files[i].d_stat.st_ctime.day,
				files[i].d_stat.st_ctime.hour,
				files[i].d_stat.st_ctime.minute,
				files[i].d_stat.st_ctime.second,
				files[i].d_stat.st_ctime.microsecond
			);
			// Don't dump acces, it change every time and fail test
		/*printf(
				"'%s': acces date : %d-%d-%d-%d-%d-%d-%d\n",
				files[i].d_name,
				files[i].d_stat.st_atime.year,
				files[i].d_stat.st_atime.month,
				files[i].d_stat.st_atime.day,
				files[i].d_stat.st_atime.hour,
				files[i].d_stat.st_atime.minute,
				files[i].d_stat.st_atime.second,
				files[i].d_stat.st_atime.microsecond
			);*/
		printf(
				"'%s': modif date : %d-%d-%d-%d-%d-%d-%d\n",
				files[i].d_name,
				files[i].d_stat.st_mtime.year,
				files[i].d_stat.st_mtime.month,
				files[i].d_stat.st_mtime.day,
				files[i].d_stat.st_mtime.hour,
				files[i].d_stat.st_mtime.minute,
				files[i].d_stat.st_mtime.second,
				files[i].d_stat.st_mtime.microsecond
			);
	}
	return 0;
}
