#!/usr/bin/env perl

my @formats = qw();

open my $swizzle_c, '<', 'source/swizzle.c' or die "Failed to open source/swizzle.c: $!.";
while (my $line = <$swizzle_c>) {
	$line =~ /^DEFINE_SWIZZLE_FUNC_PAIRS\(([^,]+),[^0-9]*([0-9]+)[^,]*,/;
	print STDERR "found format:\n => name: $1\n => type: nnc_u$2\n\n" if $1;
	push @formats, { name => $1, type => "nnc_u$2" } if $1;
}

# Generate header
print <<EOF;
/** \\file  swizzle.h
 *  \\brief Functions relating to image swizzling.
 */
#ifndef inc_nnc_swizzle_h
#define inc_nnc_swizzle_h

#include <nnc/base.h>
NNC_BEGIN

EOF

# Generate function prototypes
foreach my $format (@formats) {
	my $name = $format->{name};
	my $type = $format->{type};

	print <<EOF;
/** \\brief       Unswizzles a z-order curve $name (little endian) image to an rgba8 (native endian) image.
 *  \\param inp   Input array, should be at least of size `dim*dim*sizeof($type)`.
 *  \\param outp  Output array, should be at least of size `dim*dim*sizeof(nnc_u32)`.
 *  \\param x     Image width, must be aligned by 8.
 *  \\param y     Image height, must be aligned by 8.
 */
void nnc_unswizzle_zorder_le_${name}_to_rgba8($type *inp, nnc_u32 *outp, nnc_u16 x, nnc_u16 y);

/** \\brief       Swizzles an rgba8 (native endian) image to a z-order curve $name (little endian) image.
 *  \\param inp   Input array, should be at least of size `dim*dim*sizeof(nnc_u32)`.
 *  \\param outp  Output array, should be at least of size `dim*dim*sizeof($type)`.
 *  \\param x     Image width, must be aligned by 8.
 *  \\param y     Image height, must be aligned by 8.
 */
void nnc_swizzle_zorder_rgba8_to_le_${name}(nnc_u32 *inp, $type *outp, nnc_u16 x, nnc_u16 y);

/** \\brief       Unswizzles a z-order curve $name (little endian) image to an rgba8 (big endian) image.
 *  \\param inp   Input array, should be at least of size `dim*dim*sizeof($type)`.
 *  \\param outp  Output array, should be at least of size `dim*dim*sizeof(nnc_u32)`.
 *  \\param x     Image width, must be aligned by 8.
 *  \\param y     Image height, must be aligned by 8.
 */
void nnc_unswizzle_zorder_le_${name}_to_be_rgba8($type *inp, nnc_u32 *outp, nnc_u16 x, nnc_u16 y);

/** \\brief       Swizzles an rgba8 (big endian) image to a z-order curve $name (little endian) image.
 *  \\param inp   Input array, should be at least of size `dim*dim*sizeof(nnc_u32)`.
 *  \\param outp  Output array, should be at least of size `dim*dim*sizeof($type)`.
 *  \\param x     Image width, must be aligned by 8.
 *  \\param y     Image height, must be aligned by 8.
 */
void nnc_swizzle_zorder_be_rgba8_to_le_${name}(nnc_u32 *inp, $type *outp, nnc_u16 x, nnc_u16 y);

/** \\brief       Unswizzles a z-order curver $name (little endian) image to an rgb8 (big endian) image.
 *  \\param inp   Input array, should be at least of size `dim*dim*sizeof($type)`.
 *  \\param outp  Output array, should be at least of size `dim*dim*3*sizeof(nnc_u8)`.
 *  \\param x     Image width, must be aligned by 8.
 *  \\param y     Image height, must be aligned by 8.
 *  \\note        The output image data can be represented by the following struct:
 *               \\code
 *               struct rgb8 {
 *                 u8 r;
 *                 u8 g;
 *                 u8 b;
 *               } __attribute__((packed));
 *               \\endcode
 *  \\warning     Due to alignment issues this format is not recommended.
 */
void nnc_unswizzle_zorder_le_${name}_to_be_rgb8($type *inp, nnc_u8 *outp, nnc_u16 x, nnc_u16 y);

/** \\brief       Swizzles an rgb8 (big endian) image to a z-order curve $name (little endian) image.
 *  \\param inp   Input array, should be at least of size `dim*dim*sizeof($type)`.
 *  \\param outp  Output array, should be at least of size `dim*dim*3*sizeof(nnc_u8)`.
 *  \\param x     Image width, must be aligned by 8.
 *  \\param y     Image height, must be aligned by 8.
 *  \\note        The input image data can be represented by the following struct:
 *               \\code
 *               struct rgb8 {
 *                 u8 r;
 *                 u8 g;
 *                 u8 b;
 *               } __attribute__((packed));
 *               \\endcode
 *  \\warning     Due to alignment issues this format is not recommended.
 */
void nnc_swizzle_zorder_be_rgb8_to_le_${name}(nnc_u8 *inp, $type *outp, nnc_u16 x, nnc_u16 y);
EOF
}

# Generate footer
print <<EOF;

NNC_END
#endif
EOF


