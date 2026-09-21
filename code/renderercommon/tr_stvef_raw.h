/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_STVEF_RAW_H
#define FNTECH3_TR_STVEF_RAW_H

/* Included by each renderer's backend. Each submission in a client frame
 * keeps its own image and shader: uploading the next preview must not change
 * pixels referenced by an earlier queued quad. The pool is reused next frame
 * and released with the renderer's ordinary image/shader registration. */
static void R_UploadSTVEFRaw( int slot, int cols, int rows, const byte *data ) {
	image_t *image = tr.stvefRawImages[slot];
	if ( !image ) {
		tr.stvefRawImages[slot] = R_CreateImage( va( "*stvef-raw%i", slot ), NULL,
			(byte *)data, cols, rows, IMGFLAG_CLAMPTOEDGE | IMGFLAG_RGB |
			IMGFLAG_NOSCALE | IMGFLAG_NO_COMPRESSION );
		return;
	}
#ifndef USE_VULKAN
	GL_Bind( image );
#endif
	if ( cols != image->width || rows != image->height ) {
		image->width = image->uploadWidth = cols;
		image->height = image->uploadHeight = rows;
#ifdef USE_VULKAN
		vk_create_image( image, cols, rows, 1 );
		vk_upload_image_data( image, 0, 0, cols, rows, 1, (byte *)data, cols * rows * 4, qfalse );
#else
		qglTexImage2D( GL_TEXTURE_2D, 0, image->internalFormat, cols, rows, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl_clamp_mode );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl_clamp_mode );
#endif
	} else {
#ifdef USE_VULKAN
		vk_upload_image_data( image, 0, 0, cols, rows, 1, (byte *)data, cols * rows * 4, qtrue );
#else
		qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
#endif
	}
}

void RE_DrawSTVEFRaw( int x, int y, int w, int h, int cols, int rows,
	const byte *data, float lightValue ) {
	refDrawQuad_t quad;
	float left, top, right, bottom, s0, t0, s1, t1;
	int slot, i;
	byte shade;
	if ( !tr.registered || ri.gameId != COM_GAME_STVEF_SP ) return;
	if ( !data || cols <= 0 || rows <= 0 || cols > glConfig.maxTextureSize ||
		rows > glConfig.maxTextureSize || rows > INT_MAX / 4 / cols ||
		( cols & ( cols - 1 ) ) || ( rows & ( rows - 1 ) ) || !isfinite( lightValue ) ) {
		ri.Error( ERR_DROP, "DrawSTVEFRaw: invalid image or brightness" );
		return;
	}
	if ( tr.stvefRawFrame != tr.frameCount ) {
		tr.stvefRawFrame = tr.frameCount;
		tr.stvefRawUsed = 0;
	}
	/* Retail uploaded each image and drew it immediately, so one image was
	 * always enough and a menu was never refused. Past the pool, reuse the last
	 * slot: an earlier quad sharing it can show this image, a wrong preview
	 * rather than dropping the client out of the menu it is drawing. */
	if ( tr.stvefRawUsed < (int)ARRAY_LEN( tr.stvefRawImages ) ) {
		slot = tr.stvefRawUsed++;
	} else {
		slot = (int)ARRAY_LEN( tr.stvefRawImages ) - 1;
	}
	R_UploadSTVEFRaw( slot, cols, rows, data );
	if ( !tr.stvefRawImages[slot] ) {
		ri.Error( ERR_DROP, "DrawSTVEFRaw: image creation failed" );
		return;
	}
	if ( !tr.stvefRawShaders[slot] ) {
		tr.stvefRawShaders[slot] = R_CreateSTVEFRawShader(
			va( "*stvef-raw%i", slot ), tr.stvefRawImages[slot] );
	}
	if ( !tr.stvefRawShaders[slot] ) {
		ri.Error( ERR_DROP, "DrawSTVEFRaw: shader creation failed" );
		return;
	}
	/* Native glColor3f clamps at the GL boundary. The engine's canonical
	 * quad uses byte colors; round to the nearest representable intensity. */
	if ( lightValue < 0.0f ) lightValue = 0.0f;
	if ( lightValue > 1.0f ) lightValue = 1.0f;
	shade = (byte)( lightValue * 255.0f + 0.5f );
	left = (float)x; top = (float)y;
	right = left + (float)w; bottom = top + (float)h;
	s0 = 0.5f / cols; t0 = 0.5f / rows;
	s1 = 1.0f - s0; t1 = 1.0f - t0;
	Com_Memset( &quad, 0, sizeof( quad ) );
	quad.xy[0][0] = left;  quad.xy[0][1] = top;
	quad.xy[1][0] = right; quad.xy[1][1] = top;
	quad.xy[2][0] = right; quad.xy[2][1] = bottom;
	quad.xy[3][0] = left;  quad.xy[3][1] = bottom;
	quad.st[0][0] = s0; quad.st[0][1] = t0;
	quad.st[1][0] = s1; quad.st[1][1] = t0;
	quad.st[2][0] = s1; quad.st[2][1] = t1;
	quad.st[3][0] = s0; quad.st[3][1] = t1;
	for ( i = 0; i < 4; ++i ) {
		quad.colors[i].rgba[0] = quad.colors[i].rgba[1] = quad.colors[i].rgba[2] = shade;
		quad.colors[i].rgba[3] = 255;
	}
	quad.colorMask = 15;
	quad.shader = tr.stvefRawShaders[slot]->index;
	RE_DrawQuad( &quad );
}

#endif
