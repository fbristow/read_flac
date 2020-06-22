#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <endian.h>

void parse_file(FILE *f);

typedef unsigned char byte;

typedef struct PICTURE
{
    uint32_t picture_type;
    char *mime_type;
    char *description;
    uint32_t width;
    uint32_t height;
    uint32_t colour_depth;
    uint32_t number_of_colours;
    uint32_t picture_size;
    byte *picture;
} picture;

typedef struct VORBIS_COMMENT
{
    uint32_t vendor_length;
    char *vendor_string;
    uint32_t comment_list_length;
    char **comments;
} vorbis_comment;

typedef struct SEEKPOINT
{
    uint64_t first_sample_number;
    uint64_t offset;
    uint16_t number_of_samples;
} seekpoint;

typedef struct METADATA_BLOCK_SEEKTABLE
{
    size_t total_points;
    seekpoint *entry;
} seektable;

typedef struct METADATA_BLOCK_HEADER
{
    uint8_t  last_block;
    uint8_t  block_type;
    size_t block_length;
} metadata_header;

typedef struct METADATA_BLOCK_STREAMINFO
{
    uint16_t min_block_size;
    uint16_t max_block_size;
    uint32_t min_frame_size;
    uint32_t max_frame_size;
    uint32_t sample_rate;
    uint8_t  channels;
    uint8_t  bits_per_sample;
    uint64_t total_samples;
#define MD5_SIZE 16
    uint8_t md5[MD5_SIZE];
} streaminfo;

static void parse_metadata_header(metadata_header *, FILE *);
static void parse_block_streaminfo(streaminfo *, FILE *, size_t);
static void print_block_streaminfo(streaminfo *);
static void parse_block_seektable(seektable *, FILE *, size_t);
static void print_block_seektable(seektable *);
static void parse_vorbis_comment(vorbis_comment *, FILE *, size_t);
static void print_vorbis_comment(vorbis_comment *);
static void parse_picture(picture *, FILE *, size_t);
static void print_picture(picture *);
static void check_padding(FILE *, size_t);

char *block_types[7] = {
     "STREAMINFO",
     "PADDING",
     "APPLICATION",
     "SEEKTABLE",
     "VORBIS_COMMENT",
     "CUESHEET",
     "PICTURE"
};

int main(int argc, char **argv)
{
#define SIG_SIZE 4
    char signature[SIG_SIZE];
    FILE *f = fopen(argv[1], "r");
    fread(signature, sizeof(unsigned char), SIG_SIZE, f);
    if (strncmp("fLaC", signature, SIG_SIZE))
    {
        fprintf(stderr, "%s doesn't look like a FLAC file (invalid signature).\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    metadata_header header = {0};
    do
    {
        parse_metadata_header(&header, f);
        printf("Last header? %d\n", header.last_block);
        printf("Block type: %s\n", block_types[header.block_type]);
        printf("Block length: %ld\n", header.block_length);
        if (header.block_type == 1)
        {
            check_padding(f, header.block_length);
        }
        else
        {
            fseek(f, header.block_length, SEEK_CUR);
        }
    } while (!header.last_block);
   
    return EXIT_SUCCESS;
}

static void check_padding(FILE *f, size_t size)
{
    for (size_t i = 0 ; i < size; i++)
    {
        uint8_t b;
        fread(&b, sizeof(uint8_t), 1, f);
        assert(!b); // all bits must be 0
    }
}

static void print_picture(picture *p)
{
    printf("Picture type %d\n", p->picture_type);
    printf("MIME type: %s\n", p->mime_type);
    printf("Description: %s\n", p->description);
    printf("Width: %d\n", p->width);
    printf("Height: %d\n", p->height);
    printf("Colour depth: %d\n", p->colour_depth);
    printf("Number of colours: %d\n", p->number_of_colours);
    for (uint32_t i = 0; i < p->picture_size; i++)
    {
        printf("%02x ", p->picture[i]);
        if ((i + 1) % 30 == 0 ) printf("\n");
    }
    printf("\n");
}

static void parse_picture(picture *p, FILE *f, size_t size)
{
    uint32_t mime_type_len, desc_len;
    fread(&p->picture_type, sizeof(uint32_t), 1, f);
    p->picture_type = be32toh(p->picture_type);
    fread(&mime_type_len, sizeof(uint32_t), 1, f);
    mime_type_len = be32toh(mime_type_len);
    p->mime_type = malloc(sizeof(char) * mime_type_len + 1);
    fread(p->mime_type, sizeof(char), mime_type_len, f);
    p->mime_type[mime_type_len] = '\0';
    fread(&desc_len, sizeof(uint32_t), 1, f);
    desc_len = be32toh(desc_len);
    p->description = malloc(sizeof(char) * desc_len + 1);
    fread(p->description, sizeof(char), desc_len, f);
    fread(&p->width, sizeof(uint32_t), 1, f);
    p->width = be32toh(p->width);
    fread(&p->height, sizeof(uint32_t), 1, f);
    p->height = be32toh(p->height);
    fread(&p->colour_depth, sizeof(uint32_t), 1, f);
    p->colour_depth = be32toh(p->colour_depth);
    fread(&p->number_of_colours, sizeof(uint32_t), 1, f);
    p->number_of_colours = be32toh(p->number_of_colours);
    fread(&p->picture_size, sizeof(uint32_t), 1, f);
    p->picture_size = be32toh(p->picture_size);
    p->picture = malloc(sizeof(byte) * p->picture_size);
    fread(p->picture, sizeof(byte), p->picture_size, f);
}

static void print_vorbis_comment(vorbis_comment *comment)
{
    printf("Vendor string: %s\n", comment->vendor_string);
    printf("Comments:\n");
    for (uint32_t i = 0; i < comment->comment_list_length; i++)
    {
        printf("\tcomment[%d]: %s\n", i, comment->comments[i]);
    }
}

static void parse_vorbis_comment(vorbis_comment *comment, FILE *f, size_t size)
{
    fread(&comment->vendor_length, sizeof(uint32_t), 1, f);
    comment->vendor_string = malloc(sizeof(char) * comment->vendor_length + 1);
    fread(comment->vendor_string, sizeof(char), comment->vendor_length, f);
    comment->vendor_string[comment->vendor_length] = '\0';
    fread(&comment->comment_list_length, sizeof(uint32_t), 1, f);
    comment->comments = malloc(sizeof(char*) * comment->comment_list_length);
    for (uint32_t i = 0; i < comment->comment_list_length; i++)
    {
        uint32_t comment_length;
        fread(&comment_length, sizeof(uint32_t), 1, f);
        comment->comments[i] = malloc(sizeof(char) * comment_length + 1);
        fread(comment->comments[i], sizeof(char), comment_length, f);
        comment->comments[i][comment_length] = '\0';
    }
}

static void parse_block_seektable(seektable *table, FILE *f, size_t size)
{
    byte *header = malloc(size);
    fread(header, sizeof(byte), size, f);
    // number of seekpoints is exactly size / 18 (from docs)
    table->total_points = size / sizeof(seekpoint);
    seekpoint *point = malloc(table->total_points * sizeof(seekpoint));
    table->entry = point;
    for (uint8_t i = 0; i < table->total_points; i++)
    {
        memcpy(&point[i].first_sample_number, &header[i * 18], 8);
        memcpy(&point[i].offset, &header[i * 18 + 8], 8);
        memcpy(&point[i].number_of_samples, &header[i * 18 + 16], 2);

        point[i].first_sample_number = be64toh(point[i].first_sample_number);
        point[i].offset = be64toh(point[i].offset);
        point[i].number_of_samples = be16toh(point[i].number_of_samples);
    }
    
    free(header);
}

static void print_block_seektable(seektable *table)
{
    for (size_t i = 0; i < table->total_points; i++)
    {
        printf("    point %ld: sample_number=%lu, stream_offset=%lu, frame_samples=%u\n",
                i,
                table->entry[i].first_sample_number,
                table->entry[i].offset,
                table->entry[i].number_of_samples);
    }
}

static void parse_metadata_header(metadata_header *h, FILE *f)
{
#define METADATA_BLOCK_HEADER_SIZE 4
    byte *header = malloc(METADATA_BLOCK_HEADER_SIZE);
    fread(header, sizeof(byte), METADATA_BLOCK_HEADER_SIZE, f);

    h->last_block = header[0] >> 7;
    h->block_type = header[0] & 0x7f;
    h->block_length = header[1] << 16 | header[2] << 8 | header[3];

    free(header);
}

static void parse_block_streaminfo(streaminfo *h, FILE *f, size_t size)
{
    byte *header = malloc(size);
    fread(header, sizeof(byte), size, f);

    h->min_block_size = header[0] << 8 | header[1];
    assert(h->min_block_size >= 16);
    h->max_block_size = header[2] << 8 | header[3];
    assert(h->max_block_size <= 65535);
    h->min_frame_size = header[4] << 16 | header[5] << 8 | header[6];
    h->max_frame_size = header[7] << 16 | header[8] << 8 | header[9];
    h->sample_rate = header[10] << 12 | header[11] << 4 | header[12] >> 4;
    h->channels = ((header[12] >> 1) & 0x7) + 1;
    h->bits_per_sample = ((header[12] & 0x1) << 5 | (header[13] >> 4)) + 1;
    h->total_samples = ((uint64_t)(header[13] & 0xF)) << 32 | header[14] << 24 |
        header[15] << 16 | header[16] << 8 | header[17];

    memcpy(h->md5, &header[18], MD5_SIZE);

    free(header);
}

static void print_block_streaminfo(streaminfo *info)
{
    printf("min_blocksize: %d\n", info->min_block_size);
    printf("max_blocksize: %d\n", info->max_block_size);
    printf("min_frame_size: %d\n", info->min_frame_size);
    printf("max_frame_size: %d\n", info->max_frame_size);
    printf("sample_rate: %d\n", info->sample_rate);
    printf("channels: %d\n", info->channels);
    printf("bits_per_sample: %d\n", info->bits_per_sample);
    printf("total_samples: %ld\n", info->total_samples);
    printf("md5: ");
    for (uint8_t i = 0; i < MD5_SIZE; i++)
    {
        printf("%02x", info->md5[i]);
    }
    printf("\n");
}
