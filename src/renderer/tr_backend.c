/*
 * Wolfenstein: Enemy Territory GPL Source Code
 * Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
 *
 * ET: Legacy
 * Copyright (C) 2012-2018 ET:Legacy team <mail@etlegacy.com>
 *
 * This file is part of ET: Legacy - http://www.etlegacy.com
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ET: Legacy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ET: Legacy. If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, Wolfenstein: Enemy Territory GPL Source Code is also
 * subject to certain additional terms. You should have received a copy
 * of these additional terms immediately following the terms and conditions
 * of the GNU General Public License which accompanied the source code.
 * If not, please request a copy in writing from id Software at the address below.
 *
 * id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
 */
/**
 * @file renderer/tr_backend.c
 */

#include "tr_local.h"

backEndData_t  *backEndData;
backEndState_t backEnd;

/**
 * @var s_flipMatrix
 * @brief Convert from our coordinate system (looking down X)
 * to OpenGL's coordinate system (looking down -Z)
 */
static float s_flipMatrix[16] =
{
	0,  0, -1, 0,
	-1, 0, 0,  0,
	0,  1, 0,  0,
	0,  0, 0,  1
};

/**
 * @brief GL_Bind
 * @param[in,out] image
 */
void GL_Bind(image_t *image)
{
	int texnum;

	if (!image)
	{
		Ren_Warning("GL_Bind: NULL image\n");
		texnum = tr.defaultImage->texnum;
	}
	else
	{
		texnum = image->texnum;
	}

	if (r_noBind->integer && tr.dlightImage)            // performance evaluation option
	{
		texnum = tr.dlightImage->texnum;
	}

	if (glState.currenttextures[glState.currenttmu] != texnum)
	{
		if (image)
		{
			image->frameUsed = tr.frameCount;
		}

		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture(GL_TEXTURE_2D, texnum);
	}
}

/**
 * @brief GL_SelectTexture
 * @param[in] unit
 */
void GL_SelectTexture(int unit)
{
	if (glState.currenttmu == unit)
	{
		return;
	}

	if (unit == 0)
	{
		qglActiveTextureARB(GL_TEXTURE0_ARB);
		Ren_LogComment("glActiveTextureARB( GL_TEXTURE0_ARB )\n");
		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		Ren_LogComment("glClientActiveTextureARB( GL_TEXTURE0_ARB )\n");
	}
	else if (unit == 1)
	{
		qglActiveTextureARB(GL_TEXTURE1_ARB);
		Ren_LogComment("glActiveTextureARB( GL_TEXTURE1_ARB )\n");
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		Ren_LogComment("glClientActiveTextureARB( GL_TEXTURE1_ARB )\n");
	}
	else
	{
		Ren_Drop("GL_SelectTexture: unit = %i", unit);
	}

	glState.currenttmu = unit;
}

/*
 * @brief GL_BindMultitexture
 * @param image0
 * @param env0 - unused
 * @param image1
 * @param env1 - unused
 *
 * @note Unused
void GL_BindMultitexture(image_t *image0, GLuint env0, image_t *image1, GLuint env1)
{
    int texnum0 = image0->texnum;
    int texnum1 = image1->texnum;

    if (r_nobind->integer && tr.dlightImage)            // performance evaluation option
    {
        texnum0 = texnum1 = tr.dlightImage->texnum;
    }

    if (glState.currenttextures[1] != texnum1)
    {
        GL_SelectTexture(1);
        image1->frameUsed          = tr.frameCount;
        glState.currenttextures[1] = texnum1;
        qglBindTexture(GL_TEXTURE_2D, texnum1);
    }
    if (glState.currenttextures[0] != texnum0)
    {
        GL_SelectTexture(0);
        image0->frameUsed          = tr.frameCount;
        glState.currenttextures[0] = texnum0;
        qglBindTexture(GL_TEXTURE_2D, texnum0);
    }
}
*/

/**
 * @brief GL_Cull
 * @param[in] cullType
 */
void GL_Cull(int cullType)
{
	if (glState.faceCulling == cullType)
	{
		return;
	}

	glState.faceCulling = cullType;

	if (cullType == CT_TWO_SIDED)
	{
		qglDisable(GL_CULL_FACE);
	}
	else
	{
		qboolean cullFront;
		qglEnable(GL_CULL_FACE);

		cullFront = (cullType == CT_FRONT_SIDED);
		if (backEnd.viewParms.isMirror)
		{
			cullFront = !cullFront;
		}

		qglCullFace(cullFront ? GL_FRONT : GL_BACK);
	}
}

/**
 * @brief GL_TexEnv
 * @param[in] env
 */
void GL_TexEnv(int env)
{
	if (env == glState.texEnv[glState.currenttmu])
	{
		return;
	}

	glState.texEnv[glState.currenttmu] = env;


	switch (env)
	{
	case GL_MODULATE:
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		break;
	case GL_REPLACE:
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		break;
	case GL_DECAL:
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		break;
	case GL_ADD:
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
		break;
	default:
		Ren_Drop("GL_TexEnv: invalid env '%d' passed\n", env);
	}
}

/**
 * @brief This routine is responsible for setting the most commonly changed state in Q3.
 * @param[in] stateBits
 */
void GL_State(unsigned long stateBits)
{
	unsigned long diff = stateBits ^ glState.glStateBits;

	if (!diff)
	{
		return;
	}

	// check depthFunc bits
	if (diff & GLS_DEPTHFUNC_EQUAL)
	{
		if (stateBits & GLS_DEPTHFUNC_EQUAL)
		{
			qglDepthFunc(GL_EQUAL);
		}
		else
		{
			qglDepthFunc(GL_LEQUAL);
		}
	}

	// check blend bits
	if (diff & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS))
	{
		GLenum srcFactor, dstFactor;

		if (stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS))
		{
			switch (stateBits & GLS_SRCBLEND_BITS)
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				srcFactor = GL_ONE;     // to get warning to shut up
				Ren_Drop("GL_State: invalid src blend state bits\n");
			}

			switch (stateBits & GLS_DSTBLEND_BITS)
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				dstFactor = GL_ONE;     // to get warning to shut up
				Ren_Drop("GL_State: invalid dst blend state bits\n");
			}

			qglEnable(GL_BLEND);
			qglBlendFunc(srcFactor, dstFactor);
		}
		else
		{
			qglDisable(GL_BLEND);
		}
	}

	// check depthmask
	if (diff & GLS_DEPTHMASK_TRUE)
	{
		if (stateBits & GLS_DEPTHMASK_TRUE)
		{
			qglDepthMask(GL_TRUE);
		}
		else
		{
			qglDepthMask(GL_FALSE);
		}
	}

	// fill/line mode
	if (diff & GLS_POLYMODE_LINE)
	{
		if (stateBits & GLS_POLYMODE_LINE)
		{
			qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		else
		{
			qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
	}

	// depthtest
	if (diff & GLS_DEPTHTEST_DISABLE)
	{
		if (stateBits & GLS_DEPTHTEST_DISABLE)
		{
			qglDisable(GL_DEPTH_TEST);
		}
		else
		{
			qglEnable(GL_DEPTH_TEST);
		}
	}

	// alpha test
	if (diff & GLS_ATEST_BITS)
	{
		switch (stateBits & GLS_ATEST_BITS)
		{
		case 0:
			qglDisable(GL_ALPHA_TEST);
			break;
		case GLS_ATEST_GT_0:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GREATER, 0.0f);
			break;
		case GLS_ATEST_LT_16:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_LESS, 0.0625f);
			break;
		case GLS_ATEST_GE_16:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GEQUAL, 0.0625f);
			break;
		case GLS_ATEST_LT_32:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_LESS, 0.125f);
			break;
		case GLS_ATEST_GE_32:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GEQUAL, 0.125f);
			break;
		case GLS_ATEST_LT_64:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_LESS, 0.25f);
			break;
		case GLS_ATEST_GE_64:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GEQUAL, 0.25f);
			break;
		case GLS_ATEST_LT_128:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_LESS, 0.5f);
			break;
		case GLS_ATEST_GE_128:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GEQUAL, 0.5f);
			break;
		case GLS_ATEST_GE_192:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GEQUAL, 0.75f);
			break;
		case GLS_ATEST_GE_224:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GEQUAL, 0.875f);
			break;
		default:
			etl_assert(0);
			break;
		}
	}

	glState.glStateBits = stateBits;
}

/**
 * @brief A player has predicted a teleport, but hasn't arrived yet
 */
static void RB_Hyperspace(void)
{
	float c = (backEnd.refdef.time & 255) / 255.0f;

	qglClearColor(c, c, c, 1);
	qglClear(GL_COLOR_BUFFER_BIT);

	backEnd.isHyperspace = qtrue;
}

/**
 * @brief SetViewportAndScissor
 */
static void SetViewportAndScissor(void)
{
	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
	qglMatrixMode(GL_MODELVIEW);

	// set the window clipping
	qglViewport(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
	            backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
	qglScissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
	           backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
}

/**
 * @brief Any mirrored or portaled views have already been drawn, so prepare
 * to actually render the visible surfaces for this view
 */
void RB_BeginDrawingView(void)
{
	int clearBits = 0;

	// sync with gl if needed
	if (r_finish->integer == 1 && !glState.finishCalled)
	{
		qglFinish();
		glState.finishCalled = qtrue;
	}
	if (r_finish->integer == 0)
	{
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	// set the modelview matrix for the viewer
	SetViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State(GLS_DEFAULT);


	////////// modified to ensure one glclear() per frame at most

	// clear relevant buffers
	clearBits = 0;

	if (r_measureOverdraw->integer || r_shadows->integer == 2)
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}
	// global q3 fog volume
	else if (tr.world && tr.world->globalFog >= 0)
	{
		clearBits |= GL_DEPTH_BUFFER_BIT;
		clearBits |= GL_COLOR_BUFFER_BIT;
		//
		qglClearColor(tr.world->fogs[tr.world->globalFog].shader->fogParms.color[0] * tr.identityLight,
		              tr.world->fogs[tr.world->globalFog].shader->fogParms.color[1] * tr.identityLight,
		              tr.world->fogs[tr.world->globalFog].shader->fogParms.color[2] * tr.identityLight, 1.0);
	}
	else if (skyboxportal)
	{
		if (backEnd.refdef.rdflags & RDF_SKYBOXPORTAL)     // portal scene, clear whatever is necessary
		{
			clearBits |= GL_DEPTH_BUFFER_BIT;

			if (r_fastSky->integer || (backEnd.refdef.rdflags & RDF_NOWORLDMODEL))      // fastsky: clear color
			{   // try clearing first with the portal sky fog color, then the world fog color, then finally a default
				clearBits |= GL_COLOR_BUFFER_BIT;
				if (glfogsettings[FOG_PORTALVIEW].registered)
				{
					qglClearColor(glfogsettings[FOG_PORTALVIEW].color[0], glfogsettings[FOG_PORTALVIEW].color[1], glfogsettings[FOG_PORTALVIEW].color[2], glfogsettings[FOG_PORTALVIEW].color[3]);
				}
				else if (glfogNum > FOG_NONE && glfogsettings[FOG_CURRENT].registered)
				{
					qglClearColor(glfogsettings[FOG_CURRENT].color[0], glfogsettings[FOG_CURRENT].color[1], glfogsettings[FOG_CURRENT].color[2], glfogsettings[FOG_CURRENT].color[3]);
				}
				else
				{
					qglClearColor(0.5, 0.5, 0.5, 1.0);
				}
			}
			else                                                        // rendered sky (either clear color or draw quake sky)
			{
				if (glfogsettings[FOG_PORTALVIEW].registered)
				{
					qglClearColor(glfogsettings[FOG_PORTALVIEW].color[0], glfogsettings[FOG_PORTALVIEW].color[1], glfogsettings[FOG_PORTALVIEW].color[2], glfogsettings[FOG_PORTALVIEW].color[3]);

					if (glfogsettings[FOG_PORTALVIEW].clearscreen)        // portal fog requests a screen clear (distance fog rather than quake sky)
					{
						clearBits |= GL_COLOR_BUFFER_BIT;
					}
				}

			}
		}
		else  // world scene with portal sky, don't clear any buffers, just set the fog color if there is one
		{
			clearBits |= GL_DEPTH_BUFFER_BIT;   // this will go when I get the portal sky rendering way out in the zbuffer (or not writing to zbuffer at all)

			if (glfogNum > FOG_NONE && glfogsettings[FOG_CURRENT].registered)
			{
				if (backEnd.refdef.rdflags & RDF_UNDERWATER)
				{
					if (glfogsettings[FOG_CURRENT].mode == GL_LINEAR)
					{
						clearBits |= GL_COLOR_BUFFER_BIT;
					}

				}
				else if (!(r_portalSky->integer)) // portal skies have been manually turned off, clear bg color
				{
					clearBits |= GL_COLOR_BUFFER_BIT;
				}

				qglClearColor(glfogsettings[FOG_CURRENT].color[0], glfogsettings[FOG_CURRENT].color[1], glfogsettings[FOG_CURRENT].color[2], glfogsettings[FOG_CURRENT].color[3]);
			}
			else if (!(r_portalSky->integer)) // portal skies have been manually turned off, clear bg color
			{
				clearBits |= GL_COLOR_BUFFER_BIT;
				qglClearColor(0.5, 0.5, 0.5, 1.0);
			}
		}
	}
	else // world scene with no portal sky
	{
		clearBits |= GL_DEPTH_BUFFER_BIT;

		// we don't want to clear the buffer when no world model is specified
		if (backEnd.refdef.rdflags & RDF_NOWORLDMODEL)
		{
			clearBits &= ~GL_COLOR_BUFFER_BIT;
		}
		else if (r_fastSky->integer || (backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
		{

			clearBits |= GL_COLOR_BUFFER_BIT;

			if (glfogsettings[FOG_CURRENT].registered)     // try to clear fastsky with current fog color
			{
				qglClearColor(glfogsettings[FOG_CURRENT].color[0], glfogsettings[FOG_CURRENT].color[1], glfogsettings[FOG_CURRENT].color[2], glfogsettings[FOG_CURRENT].color[3]);
			}
			else
			{
				qglClearColor(0.05f, 0.05f, 0.05f, 1.0f);    // JPW NERVE changed per id req was 0.5s
			}
		}
		else  // world scene, no portal sky, not fastsky, clear color if fog says to, otherwise, just set the clearcolor
		{
			if (glfogsettings[FOG_CURRENT].registered)     // try to clear fastsky with current fog color
			{
				qglClearColor(glfogsettings[FOG_CURRENT].color[0], glfogsettings[FOG_CURRENT].color[1], glfogsettings[FOG_CURRENT].color[2], glfogsettings[FOG_CURRENT].color[3]);

				if (glfogsettings[FOG_CURRENT].clearscreen)       // world fog requests a screen clear (distance fog rather than quake sky)
				{
					clearBits |= GL_COLOR_BUFFER_BIT;
				}
			}
		}
	}

	// don't clear the color buffer when no world model is specified
	if (backEnd.refdef.rdflags & RDF_NOWORLDMODEL)
	{
		clearBits &= ~GL_COLOR_BUFFER_BIT;
	}

	if (clearBits)
	{
		qglClear(clearBits);
	}

	if ((backEnd.refdef.rdflags & RDF_HYPERSPACE))
	{
		RB_Hyperspace();
		return;
	}
	else
	{
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1; // force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	// clip to the plane of the portal
	if (backEnd.viewParms.isPortal)
	{
		float  plane[4];
		double plane2[4]; // keep this, glew expects double

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct(backEnd.viewParms.orientation.axis[0], plane);
		plane2[1] = DotProduct(backEnd.viewParms.orientation.axis[1], plane);
		plane2[2] = DotProduct(backEnd.viewParms.orientation.axis[2], plane);
		plane2[3] = DotProduct(plane, backEnd.viewParms.orientation.origin) - plane[3];

		qglLoadMatrixf(s_flipMatrix);
		qglClipPlane(GL_CLIP_PLANE0, plane2);
		qglEnable(GL_CLIP_PLANE0);
	}
	else
	{
		qglDisable(GL_CLIP_PLANE0);
	}
}

/**
 * @brief RB_RenderDrawSurfList
 * @param[in] drawSurfs
 * @param[in] numDrawSurfs
 */
void RB_RenderDrawSurfList(drawSurf_t *drawSurfs, int numDrawSurfs)
{
	shader_t   *shader, *oldShader;
	int        fogNum, oldFogNum;
	int        entityNum, oldEntityNum;
	int        frontFace;
	int        dlighted, oldDlighted;
	qboolean   depthRange, oldDepthRange;
	int        i;
	drawSurf_t *drawSurf;
	int        oldSort;
	double     originalTime = backEnd.refdef.floatTime; // save original time for entity shader offsets

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView();

	// draw everything
	oldEntityNum          = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader             = NULL;
	oldFogNum             = -1;
	oldDepthRange         = qfalse;
	oldDlighted           = qfalse;
	oldSort               = -1;
	depthRange            = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++)
	{
		if (drawSurf->sort == oldSort)
		{
			// fast path, same as previous sort
			rb_surfaceTable[*drawSurf->surface] (drawSurf->surface);
			continue;
		}
		oldSort = drawSurf->sort;
		R_DecomposeSort(drawSurf->sort, &entityNum, &shader, &fogNum, &frontFace, &dlighted);

		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if (shader && (shader != oldShader || fogNum != oldFogNum || dlighted != oldDlighted
		               || (entityNum != oldEntityNum && !shader->entityMergable)))
		{
			if (oldShader != NULL)
			{
				RB_EndSurface();
			}
			RB_BeginSurface(shader, fogNum);
			oldShader   = shader;
			oldFogNum   = fogNum;
			oldDlighted = dlighted;
		}

		// change the modelview matrix if needed
		if (entityNum != oldEntityNum)
		{
			depthRange = qfalse;

			if (entityNum != ENTITYNUM_WORLD)
			{
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];

				// FIXME: e.shaderTime must be passed as int to avoid fp-precision loss issues
				backEnd.refdef.floatTime = originalTime; // - backEnd.currentEntity->e.shaderTime; // JPW NERVE pulled this to match q3ta

				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				// tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.orientation);

				// set up the dynamic lighting if needed
				if (backEnd.currentEntity->needDlights)
				{
					R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.orientation);
				}

				if (backEnd.currentEntity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else
			{
				backEnd.currentEntity    = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.orientation      = backEnd.viewParms.world;

				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				// tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.orientation);
			}

			qglLoadMatrixf(backEnd.orientation.modelMatrix);

			// change depthrange if needed
			if (oldDepthRange != depthRange)
			{
				if (depthRange)
				{
					qglDepthRange(0, 0.3);
				}
				else
				{
					qglDepthRange(0, 1);
				}
				oldDepthRange = depthRange;
			}

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[*drawSurf->surface] (drawSurf->surface);
	}

	// draw the contents of the last shader batch
	if (oldShader != NULL)
	{
		RB_EndSurface();
	}

	// go back to the world modelview matrix
	backEnd.currentEntity    = &tr.worldEntity;
	backEnd.refdef.floatTime = originalTime;
	backEnd.orientation      = backEnd.viewParms.world;
	R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.orientation);

	qglLoadMatrixf(backEnd.viewParms.world.modelMatrix);
	if (depthRange)
	{
		qglDepthRange(0, 1);
	}

	// draw sun
	RB_DrawSun();

	// darken down any stencil shadows
	RB_ShadowFinish();

	// add light flares on lights that aren't obscured
	RB_RenderFlares();
}

/*
============================================================================
RENDER BACK END FUNCTIONS
============================================================================
*/

/**
 * @brief RB_SetGL2D
 */
void RB_SetGL2D(void)
{
	backEnd.projection2D = qtrue;

	// set 2D virtual screen size
	qglViewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	qglScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	GL_State(GLS_DEPTHTEST_DISABLE |
	         GLS_SRCBLEND_SRC_ALPHA |
	         GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);

	qglDisable(GL_CULL_FACE);
	qglDisable(GL_CLIP_PLANE0);

	// set time for 2D shaders
	backEnd.refdef.time      = ri.Milliseconds();
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001;
}

/**
 * @brief Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
 * Used for cinematics.
 *
 * @param[in] x
 * @param[in] y
 * @param[in] w
 * @param[in] h
 * @param[in] cols
 * @param[in] rows
 * @param[in] data
 * @param[in] client
 * @param[in] dirty
 *
 * @todo FIXME: not exactly backend
 */
void RE_StretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
	int i, j;
	int start;

	if (!tr.registered)
	{
		return;
	}
	R_IssuePendingRenderCommands();

	// we definately want to sync every frame for the cinematics
	qglFinish();

	start = 0;
	if (r_speeds->integer)
	{
		start = ri.Milliseconds();
	}

	if (!GL_ARB_texture_non_power_of_two)
	{
		// make sure rows and cols are powers of 2
		for (i = 0; (1 << i) < cols; i++)
		{
		}
		for (j = 0; (1 << j) < rows; j++)
		{
		}
		if ((1 << i) != cols || (1 << j) != rows)
		{
			Ren_Drop("Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
		}
	}

	GL_Bind(tr.scratchImage[client]);

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if (cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height)
	{
		tr.scratchImage[client]->width  = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D(GL_TEXTURE_2D, 0, 3, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
	{
		if (dirty)
		{
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
	}

	if (r_speeds->integer)
	{
		int end = ri.Milliseconds();

		Ren_Print("qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start);
	}

	RB_SetGL2D();

	qglColor3f(tr.identityLight, tr.identityLight, tr.identityLight);

	qglBegin(GL_QUADS);
	qglTexCoord2f(0.5f / cols, 0.5f / rows);
	qglVertex2f(x, y);
	qglTexCoord2f((cols - 0.5f) / cols, 0.5f / rows);
	qglVertex2f(x + w, y);
	qglTexCoord2f((cols - 0.5f) / cols, (rows - 0.5f) / rows);
	qglVertex2f(x + w, y + h);
	qglTexCoord2f(0.5f / cols, (rows - 0.5f) / rows);
	qglVertex2f(x, y + h);
	qglEnd();
}

/**
 * @brief RE_UploadCinematic
 *
 * @param w - unused
 * @param h - unused
 * @param[in] cols
 * @param[in] rows
 * @param[in] data
 * @param[in] client
 * @param[in] dirty
 */
void RE_UploadCinematic(int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
	GL_Bind(tr.scratchImage[client]);

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if (cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height)
	{
		tr.scratchImage[client]->width  = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D(GL_TEXTURE_2D, 0, 3, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
	{
		if (dirty)
		{
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
	}
}

/**
 * @brief RB_SetColor
 * @param[in] data
 * @return
 */
const void *RB_SetColor(const void *data)
{
	const setColorCommand_t *cmd = ( const setColorCommand_t * ) data;

	backEnd.color2D[0] = (byte)(cmd->color[0] * 255);
	backEnd.color2D[1] = (byte)(cmd->color[1] * 255);
	backEnd.color2D[2] = (byte)(cmd->color[2] * 255);
	backEnd.color2D[3] = (byte)(cmd->color[3] * 255);

	return ( const void * ) (cmd + 1);
}

/**
 * @brief RB_StretchPic
 * @param[in] data
 * @return
 */
const void *RB_StretchPic(const void *data)
{
	const stretchPicCommand_t *cmd = ( const stretchPicCommand_t * ) data;
	shader_t                  *shader;
	int                       numVerts, numIndexes;

	if (!backEnd.projection2D)
	{
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if (shader != tess.shader)
	{
		if (tess.numIndexes)
		{
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface(shader, 0);
	}

	RB_CHECKOVERFLOW(4, 6);
	numVerts   = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes  += 6;

	tess.indexes[numIndexes]     = numVerts + 3;
	tess.indexes[numIndexes + 1] = numVerts + 0;
	tess.indexes[numIndexes + 2] = numVerts + 2;
	tess.indexes[numIndexes + 3] = numVerts + 2;
	tess.indexes[numIndexes + 4] = numVerts + 0;
	tess.indexes[numIndexes + 5] = numVerts + 1;

	*( int * ) tess.vertexColors[numVerts]                 =
	    *( int * ) tess.vertexColors[numVerts + 1]         =
	        *( int * ) tess.vertexColors[numVerts + 2]     =
	            *( int * ) tess.vertexColors[numVerts + 3] = *( int * ) backEnd.color2D;

	tess.xyz[numVerts][0] = cmd->x;
	tess.xyz[numVerts][1] = cmd->y;
	tess.xyz[numVerts][2] = 0;

	tess.texCoords[numVerts][0][0] = cmd->s1;
	tess.texCoords[numVerts][0][1] = cmd->t1;

	tess.xyz[numVerts + 1][0] = cmd->x + cmd->w;
	tess.xyz[numVerts + 1][1] = cmd->y;
	tess.xyz[numVerts + 1][2] = 0;

	tess.texCoords[numVerts + 1][0][0] = cmd->s2;
	tess.texCoords[numVerts + 1][0][1] = cmd->t1;

	tess.xyz[numVerts + 2][0] = cmd->x + cmd->w;
	tess.xyz[numVerts + 2][1] = cmd->y + cmd->h;
	tess.xyz[numVerts + 2][2] = 0;

	tess.texCoords[numVerts + 2][0][0] = cmd->s2;
	tess.texCoords[numVerts + 2][0][1] = cmd->t2;

	tess.xyz[numVerts + 3][0] = cmd->x;
	tess.xyz[numVerts + 3][1] = cmd->y + cmd->h;
	tess.xyz[numVerts + 3][2] = 0;

	tess.texCoords[numVerts + 3][0][0] = cmd->s1;
	tess.texCoords[numVerts + 3][0][1] = cmd->t2;

	return ( const void * ) (cmd + 1);
}

/**
 * @brief RB_Draw2dPolys
 * @param[in] data
 * @return
 */
const void *RB_Draw2dPolys(const void *data)
{
	const poly2dCommand_t *cmd = ( const poly2dCommand_t * ) data;
	shader_t              *shader;
	int                   i;

	if (!backEnd.projection2D)
	{
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if (shader != tess.shader)
	{
		if (tess.numIndexes)
		{
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface(shader, 0);
	}

	RB_CHECKOVERFLOW(cmd->numverts, (cmd->numverts - 2) * 3);

	for (i = 0; i < cmd->numverts - 2; i++)
	{
		tess.indexes[tess.numIndexes + 0] = tess.numVertexes;
		tess.indexes[tess.numIndexes + 1] = tess.numVertexes + i + 1;
		tess.indexes[tess.numIndexes + 2] = tess.numVertexes + i + 2;
		tess.numIndexes                  += 3;
	}

	for (i = 0; i < cmd->numverts; i++)
	{
		tess.xyz[tess.numVertexes][0] = cmd->verts[i].xyz[0];
		tess.xyz[tess.numVertexes][1] = cmd->verts[i].xyz[1];
		tess.xyz[tess.numVertexes][2] = 0;

		tess.texCoords[tess.numVertexes][0][0] = cmd->verts[i].st[0];
		tess.texCoords[tess.numVertexes][0][1] = cmd->verts[i].st[1];

		tess.vertexColors[tess.numVertexes][0] = cmd->verts[i].modulate[0];
		tess.vertexColors[tess.numVertexes][1] = cmd->verts[i].modulate[1];
		tess.vertexColors[tess.numVertexes][2] = cmd->verts[i].modulate[2];
		tess.vertexColors[tess.numVertexes][3] = cmd->verts[i].modulate[3];
		tess.numVertexes++;
	}

	return ( const void * ) (cmd + 1);
}

/**
 * @brief RB_RotatedPic
 * @param[in] data
 * @return
 */
const void *RB_RotatedPic(const void *data)
{
	const stretchPicCommand_t *cmd = ( const stretchPicCommand_t * ) data;
	shader_t                  *shader;
	int                       numVerts, numIndexes;
	float                     angle;

	if (!backEnd.projection2D)
	{
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if (shader != tess.shader)
	{
		if (tess.numIndexes)
		{
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface(shader, 0);
	}

	RB_CHECKOVERFLOW(4, 6);
	numVerts   = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes  += 6;

	tess.indexes[numIndexes]     = numVerts + 3;
	tess.indexes[numIndexes + 1] = numVerts + 0;
	tess.indexes[numIndexes + 2] = numVerts + 2;
	tess.indexes[numIndexes + 3] = numVerts + 2;
	tess.indexes[numIndexes + 4] = numVerts + 0;
	tess.indexes[numIndexes + 5] = numVerts + 1;

	*( int * ) tess.vertexColors[numVerts]                 =
	    *( int * ) tess.vertexColors[numVerts + 1]         =
	        *( int * ) tess.vertexColors[numVerts + 2]     =
	            *( int * ) tess.vertexColors[numVerts + 3] = *( int * ) backEnd.color2D;

	angle                 = cmd->angle * M_TAU_F;
	tess.xyz[numVerts][0] = cmd->x + (cos(angle) * cmd->w);
	tess.xyz[numVerts][1] = cmd->y + (sin(angle) * cmd->h);
	tess.xyz[numVerts][2] = 0;

	tess.texCoords[numVerts][0][0] = cmd->s1;
	tess.texCoords[numVerts][0][1] = cmd->t1;

	angle                     = cmd->angle * M_TAU_F + 0.25 * M_TAU_F;
	tess.xyz[numVerts + 1][0] = cmd->x + (cos(angle) * cmd->w);
	tess.xyz[numVerts + 1][1] = cmd->y + (sin(angle) * cmd->h);
	tess.xyz[numVerts + 1][2] = 0;

	tess.texCoords[numVerts + 1][0][0] = cmd->s2;
	tess.texCoords[numVerts + 1][0][1] = cmd->t1;

	angle                     = cmd->angle * M_TAU_F + 0.50 * M_TAU_F;
	tess.xyz[numVerts + 2][0] = cmd->x + (cos(angle) * cmd->w);
	tess.xyz[numVerts + 2][1] = cmd->y + (sin(angle) * cmd->h);
	tess.xyz[numVerts + 2][2] = 0;

	tess.texCoords[numVerts + 2][0][0] = cmd->s2;
	tess.texCoords[numVerts + 2][0][1] = cmd->t2;

	angle                     = cmd->angle * M_TAU_F + 0.75 * M_TAU_F;
	tess.xyz[numVerts + 3][0] = cmd->x + (cos(angle) * cmd->w);
	tess.xyz[numVerts + 3][1] = cmd->y + (sin(angle) * cmd->h);
	tess.xyz[numVerts + 3][2] = 0;

	tess.texCoords[numVerts + 3][0][0] = cmd->s1;
	tess.texCoords[numVerts + 3][0][1] = cmd->t2;

	return ( const void * ) (cmd + 1);
}

/**
 * @brief RB_StretchPicGradient
 * @param[in] data
 * @return
 */
const void *RB_StretchPicGradient(const void *data)
{
	const stretchPicCommand_t *cmd = ( const stretchPicCommand_t * ) data;
	shader_t                  *shader;
	int                       numVerts, numIndexes;

	if (!backEnd.projection2D)
	{
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if (shader != tess.shader)
	{
		if (tess.numIndexes)
		{
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface(shader, 0);
	}

	RB_CHECKOVERFLOW(4, 6);
	numVerts   = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes  += 6;

	tess.indexes[numIndexes]     = numVerts + 3;
	tess.indexes[numIndexes + 1] = numVerts + 0;
	tess.indexes[numIndexes + 2] = numVerts + 2;
	tess.indexes[numIndexes + 3] = numVerts + 2;
	tess.indexes[numIndexes + 4] = numVerts + 0;
	tess.indexes[numIndexes + 5] = numVerts + 1;

	*( int * ) tess.vertexColors[numVerts]         =
	    *( int * ) tess.vertexColors[numVerts + 1] = *( int * ) backEnd.color2D;

	*( int * ) tess.vertexColors[numVerts + 2]     =
	    *( int * ) tess.vertexColors[numVerts + 3] = *( int * ) cmd->gradientColor;

	tess.xyz[numVerts][0] = cmd->x;
	tess.xyz[numVerts][1] = cmd->y;
	tess.xyz[numVerts][2] = 0;

	tess.texCoords[numVerts][0][0] = cmd->s1;
	tess.texCoords[numVerts][0][1] = cmd->t1;

	tess.xyz[numVerts + 1][0] = cmd->x + cmd->w;
	tess.xyz[numVerts + 1][1] = cmd->y;
	tess.xyz[numVerts + 1][2] = 0;

	tess.texCoords[numVerts + 1][0][0] = cmd->s2;
	tess.texCoords[numVerts + 1][0][1] = cmd->t1;

	tess.xyz[numVerts + 2][0] = cmd->x + cmd->w;
	tess.xyz[numVerts + 2][1] = cmd->y + cmd->h;
	tess.xyz[numVerts + 2][2] = 0;

	tess.texCoords[numVerts + 2][0][0] = cmd->s2;
	tess.texCoords[numVerts + 2][0][1] = cmd->t2;

	tess.xyz[numVerts + 3][0] = cmd->x;
	tess.xyz[numVerts + 3][1] = cmd->y + cmd->h;
	tess.xyz[numVerts + 3][2] = 0;

	tess.texCoords[numVerts + 3][0][0] = cmd->s1;
	tess.texCoords[numVerts + 3][0][1] = cmd->t2;

	return ( const void * ) (cmd + 1);
}

#if 0
#define R_GetGLError() R_PrintGLError(__FILE__, __LINE__)
#else
#define R_GetGLError()
#endif

void R_PrintGLError(const char *sourceFilename, int sourceLine)
{
	GLenum err;
	if ((err = glGetError()) != GL_NO_ERROR)
	{
		Com_Printf("%s:%i: glGetError() == 0x%x\n", sourceFilename, sourceLine, err);
	}		
}
static float HaltonSequence(int index, int base)
{
	float f = 1.f;
	float r = 0.f;

	while (index > 0)
	{
		f /= base;
		r += f * (index % base);
		index /= base;
	}

	return r;
}
void DrawStuff_GL430(void)
{
	float positions[] = { -1.f, 1.f,  -1.f, -1.f,  1.f, -1.f,  1.f, 1.f };
	GLuint vbo[2], vao;
	glGenBuffers(2, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, positions, GL_STATIC_DRAW);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

	glBindVertexArray(vao);
	//glClearColor(0.5f, 0.5f, 0.f, 1.f);
	//glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_QUADS, 0, 4);

	glDisableVertexAttribArray(0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
void DrawStuff(int width, int height)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0.f, 0.f); glVertex2i(0, height);
	glTexCoord2f(0.f, 1.f); glVertex2i(0, 0);
	glTexCoord2f(1.f, 1.f); glVertex2i(width, 0);
	glTexCoord2f(1.f, 0.f); glVertex2i(width, height);
	glEnd();
}
static void DrawStuffDynamicRes(int width, int height, float scale)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0.f, 0.f); glVertex2i(0, height);
	glTexCoord2f(0.f, 1.f); glVertex2i(0, 0 - height / scale + height);
	glTexCoord2f(1.f, 1.f); glVertex2i(width / scale, 0 - height / scale + height);
	glTexCoord2f(1.f, 0.f); glVertex2i(width / scale, height);
	glEnd();
}
static void BindTexture(GLuint texnum)
{
	if (glState.currenttextures[glState.currenttmu] != texnum)
	{
		glBindTexture(GL_TEXTURE_2D, texnum);
		glState.currenttextures[glState.currenttmu] = texnum;
	}
}
static void RB_PerformFxaa(int width, int height, GLuint inputTexture, GLuint outputFramebuffer)
{
	qglViewport(0, 0, width, height);
	qglScissor(0, 0, width, height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, width, height, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();
	GL_State(GLS_DEPTHTEST_DISABLE);
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_CLIP_PLANE0);

	glBindFramebuffer(GL_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[1]);
	BindTexture(inputTexture);

	glUseProgram(backEnd.objects.resScaleProgramObjects[PO_FXAA]);
	glUniform1i(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_FXAA], "Texture"), 0);
	glUniform2f(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_FXAA], "TextureSize"), width, height);

	DrawStuff(width, height);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[1]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, outputFramebuffer);
	glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

/**
* @brief RB_SetupResolutionScaleBuffers
* @return
*/
void RB_SetupResolutionScaleBuffers(void)
{
	static const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
	GLuint multisample;
	int renderWidth, renderHeight;
	int i;

	if (!glConfigExt.multisampleSamples && glConfigExt.maxSamples > 0)
	{
		if (r_fboMultisample->integer == -1)
		{
			multisample = glConfigExt.maxSamples;
		}
		else
		{
			multisample = min(r_fboMultisample->integer, glConfigExt.maxSamples);
		}
	}
	else
	{
		multisample = 0;
	}

	renderWidth = glConfig.vidWidth * r_resolutionScale->value * (r_simpleSupersample->integer ? 2 : 1);
	renderHeight = glConfig.vidHeight * r_resolutionScale->value * (r_simpleSupersample->integer ? 2 : 1);

	glBindFramebuffer(GL_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[1]);
	glBindRenderbuffer(GL_RENDERBUFFER, backEnd.objects.resScaleColorBuffers[1]);
	glRenderbufferStorage(GL_RENDERBUFFER, glConfigExt.framebufferSrgbAvailable ? GL_SRGB : GL_RGB, renderWidth, renderHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, backEnd.objects.resScaleColorBuffers[1]);

	R_GetGLError();
	for (i = 0; i < ARRAY_LEN(backEnd.objects.resScaleFramebuffersMultisample); i++)
	{
		GLuint texTarget;
		glBindFramebuffer(GL_FRAMEBUFFER, backEnd.objects.resScaleFramebuffersMultisample[i]);
		R_GetGLError();
		if (multisample > 0)
		{
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, backEnd.objects.resScaleTexturesMultisample[i]);
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, multisample, glConfigExt.framebufferSrgbAvailable ? (i ? GL_SRGB_ALPHA : GL_SRGB) : GL_RGB, renderWidth, renderHeight, GL_FALSE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, backEnd.objects.resScaleTexturesMultisample[i], 0);
		}
		else
		{
			glBindRenderbuffer(GL_RENDERBUFFER, backEnd.objects.resScaleColorBufferMultisample);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, drawBuffer, GL_RENDERBUFFER, backEnd.objects.resScaleColorBufferMultisample);
			//glTexImage2D(texTarget, 0, glConfigExt.framebufferSrgbAvailable ? GL_SRGB : GL_RGB, renderWidth, renderHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glRenderbufferStorage(GL_RENDERBUFFER, glConfigExt.framebufferSrgbAvailable ? GL_SRGB : GL_RGB, renderWidth, renderHeight);
		}
		if (multisample > 0)
		{
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, backEnd.objects.resScaleDepthTexturesMultisample[i]);
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, multisample, GL_DEPTH24_STENCIL8, renderWidth, renderHeight, GL_FALSE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, backEnd.objects.resScaleDepthTexturesMultisample[0], 0);
		}
		else
		{
			//glTexImage2D(texTarget, 0, GL_DEPTH24_STENCIL8, renderWidth, renderHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_BYTE, NULL);
			glBindRenderbuffer(GL_RENDERBUFFER, backEnd.objects.resScaleDepthBufferMultisample);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, renderWidth, renderHeight);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, backEnd.objects.resScaleDepthBufferMultisample);
		}
		glDrawBuffer(drawBuffer);
		//if (r_srgbGammaCorrect->integer != 2)
		{
			break;
		}
	}

	for (i = 0; i < ARRAY_LEN(backEnd.objects.resScaleTextures); i++)
	{
		BindTexture(backEnd.objects.resScaleTextures[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (i == 0)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, glConfigExt.framebufferSrgbAvailable ? GL_SRGB : GL_RGB, renderWidth, renderHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[0]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, backEnd.objects.resScaleTextures[i], 0);
		}
		else if (i == 1)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, glConfigExt.framebufferSrgbAvailable ? GL_SRGB : GL_RGB, renderWidth / r_resolutionScale->integer, renderHeight / r_resolutionScale->integer, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[2]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, backEnd.objects.resScaleTextures[i], 0);
		}
		else if (i == 2)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, glConfigExt.framebufferSrgbAvailable ? GL_SRGB : GL_RGB, glConfig.vidWidth * (r_highQualityScaling->integer & 2 ? r_resolutionScale->integer : r_resolutionScale->value), glConfig.vidHeight * (r_highQualityScaling->integer & 2 ? r_resolutionScale->integer : r_resolutionScale->value), 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[3]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, backEnd.objects.resScaleTextures[i], 0);
		}
	}

	backEnd.objects.resScaleWeightsCalculated = qfalse;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static float RB_CalcBicubicWeight(float x, int mode)
{
	float B,C;

	switch (mode)
	{
	case 5:
		// Mitchel-Netravali coefficients.
		// Best psychovisual result.
		B = 1.0 / 3.0;
		C = 1.0 / 3.0;
		break;
	case 4:
		// Sharper version.
		// May look better in some cases.
		B = 0.0;
		C = 0.75;
		break;
	default:
		B = 0.1;
		C = 0.5;
		break;
	}
	if (x < 1.0)
	{
		return
			(
				pow(x, 2.0) * ((12.0 - 9.0 * B - 6.0 * C) * x + (-18.0 + 12.0 * B + 6.0 * C)) +
				(6.0 - 2.0 * B)
				) / 6.0;
	}
	else if ((x >= 1.0) && (x < 2.0))
	{
		return
			(
				pow(x, 2.0) * ((-B - 6.0 * C) * x + (6.0 * B + 30.0 * C)) +
				(-12.0 * B - 48.0 * C) * x + (8.0 * B + 24.0 * C)
				) / 6.0;
	}
	else
	{
		return 0.0;
	}
}
void RB_CalcScalingWeights(int mode, float sampleRadius, int numPatterns, int numJitters, const vec2_t *jitters, weightBuffer_t *weights)
{
	float radius;
	switch (mode)
	{
	case 1:
		radius = sampleRadius;
		break;
	default:
		radius = 2.f;
		break;
	}
	vec2_t stepxy = {1.f / backEnd.viewParms.viewportWidth, 1.f / backEnd.viewParms.viewportHeight};
	vec2_t outpix = {1.f / glConfig.vidWidth, 1.f / glConfig.vidHeight};
	int i, j, k;
	int weightIndex = 0;
	for (i = 0; i < numPatterns; i++)
	{
		for (j = 0; j < numPatterns; j++)
		{
			vec2_t fpos_org = {(0.5 - radius + i) * outpix[0], (0.5 - radius + j) * outpix[1]};
			weights->indexes[i][j] = weightIndex;
			for (k = 0; k < numJitters; k++)
			{
				vec2_t fpos = {fpos_org[0] - jitters[k][0] * stepxy[0], fpos_org[1] - jitters[k][1] * stepxy[1]};
				fpos[0] -= (fpos[0] / stepxy[0] - floor(fpos[0] / stepxy[0])) * stepxy[0];
				fpos[1] -= (fpos[1] / stepxy[1] - floor(fpos[1] / stepxy[1])) * stepxy[1];
				fpos[0] += 0.5 * stepxy[0];
				fpos[1] += 0.5 * stepxy[1];
				if (fpos[0] <= fpos_org[0])
				{
					fpos[0] += stepxy[0];
				}
				if (fpos[1] <= fpos_org[1])
				{
					fpos[1] += stepxy[1];
				}
				vec2_t maxpt = {(0.5 + radius + i) * outpix[0], (0.5 + radius + j) * outpix[1]};
				vec2_t cpos;
				weights->FirstPos[i][j][k][0] = fpos[0] - (i + 0.5) * outpix[0];
				weights->FirstPos[i][j][k][1] = fpos[1] - (j + 0.5) * outpix[1];

				for (cpos[1] = fpos[1], weights->counter[i][j][k][1] = 0; cpos[1] < maxpt[1]; cpos[1] += stepxy[1], weights->counter[i][j][k][1]++)
				{
					double distY = cpos[1] / outpix[1] - j - 0.5;

					for (cpos[0] = fpos[0], weights->counter[i][j][k][0] = 0; cpos[0] < maxpt[0]; cpos[0] += stepxy[0], weightIndex++, weights->counter[i][j][k][0]++)
					{
						double distX = cpos[0] / outpix[0] - i - 0.5;
						double dist2D = sqrt(distX * distX + distY * distY);
						double sample;
						if (dist2D >= radius)
						{
							weights->buffer[weightIndex] = 0.f;
							continue;
						}
						else if(dist2D == 0.0)
						{
							weights->buffer[weightIndex] = 1.f;
							continue;
						}
						switch (mode)
						{
						case 1:
							sample = max(dist2D * 3.1415926535897932384626433832795, 1e-5);
							weights->buffer[weightIndex] = sin(sample) / sample * sin(sample / radius) / (sample / radius);
							break;
						default :
							weights->buffer[weightIndex] = RB_CalcBicubicWeight(dist2D, mode);
							break;
						}
					}
				}
			}
		}
	}
}

void RB_CalcSupersampleWeights(const vec2_t *jitters, int numJitters, float radius, float *weights)
{
	double dist, sample;
	double f_pos[2], cpos[2];
	int i, j;

	j = 0;
	for (i = 0; i < numJitters; i++)
	{
		f_pos[0] = -(double)radius - jitters[i][0];
		f_pos[1] = -(double)radius - jitters[i][1];
		if (f_pos[0] <= -radius)
		{
			f_pos[0] += 1.0;
		}
		if (f_pos[1] <= -radius)
		{
			f_pos[1] += 1.0;
		}

		for (cpos[1] = f_pos[1]; cpos[1] < radius; cpos[1] += 1.0)
		{
			for (cpos[0] = f_pos[0]; cpos[0] < radius; cpos[0] += 1.0)
			{
				dist = sqrtf(powf(cpos[0], 2.0) + powf(cpos[1], 2.0));

				if (dist >= radius)
				{
					weights[j] = 0.0;
				}
				else if (dist == 0.0)
				{
					weights[j] = 1.0;
				}
				else
				{
					//sample = dist * 3.1415926535897932384626433832795;
					sample = max(dist * 3.1415926535897932384626433832795, 1e-5);
					weights[j] = sin(sample) / sample * sin(sample / radius) / (sample / radius);
				}
				j++;
			}
		}
	}
}

void RB_SetupSupersampleBuffers(void)
{
	int i, j, count;

	for (i = 0; i < r_supersample->integer; i++)
	{
		BindTexture(backEnd.objects.supersampleTextures[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, glConfigExt.framebufferSrgbAvailable ? GL_SRGB : GL_RGB, glConfig.vidWidth * r_resolutionScale->value, glConfig.vidHeight * r_resolutionScale->value, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, backEnd.objects.supersampleFramebuffers[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, backEnd.objects.supersampleTextures[i], 0);
	}
	for (i = 0, count = 0; i < min(r_supersampleMultiframe->integer, ARRAY_LEN(backEnd.objects.supersampleWeights)); i++)
	{
		for (j = 0; j < r_supersample->integer; j++, count++)
		{
			backEnd.objects.supersampleOffsets[i][j][0] = HaltonSequence(count, 2);
			backEnd.objects.supersampleOffsets[i][j][1] = HaltonSequence(count, 3);
			backEnd.objects.supersampleJittersInPixel[i][j][0] = (backEnd.objects.supersampleOffsets[i][j][0] - 0.5) * r_supersampleSmoothness->value;
			backEnd.objects.supersampleJittersInPixel[i][j][1] = (backEnd.objects.supersampleOffsets[i][j][1] - 0.5) * r_supersampleSmoothness->value;
		}
		RB_CalcSupersampleWeights(backEnd.objects.supersampleJittersInPixel, r_supersample->integer, r_scalingSampleRadius->value, backEnd.objects.supersampleWeights[i]);
	}
}

static float dynamicScale = 1.f;
void RB_PrepairOffScreenBuffers(void)
{
	static qboolean resScaleModified = qfalse;
	//static float resScaleOrg = 0.0;
	static int resScaleModCount = 0, fboMsModCount = 0, scalingSampleRadius = 0, HQScalingModCount = 0, HQScalingModeModCount = 0, supersampleModCount = 0, supersampleSmoothenessModCount = 0, supersampleMultiframeModCount = 0, supersampleModeModCount = 0,
		resScaleLodFixModCount = 0, simpleSupersampleModCount = 0;
	int i;

	if (r_resolutionScale->modificationCount != resScaleModCount || r_fboMultisample->modificationCount != fboMsModCount || scalingSampleRadius != r_scalingSampleRadius->modificationCount || HQScalingModCount != r_highQualityScaling->modificationCount || HQScalingModeModCount != r_highQualityScalingMode->modificationCount ||
		supersampleModCount != r_supersample->modificationCount || supersampleSmoothenessModCount != r_supersampleSmoothness->modificationCount || supersampleMultiframeModCount != r_supersampleMultiframe->modificationCount || supersampleModeModCount != r_supersampleMode->modificationCount ||
		resScaleLodFixModCount != r_resolutionScaleLodFix->modificationCount || r_simpleSupersample->modificationCount != simpleSupersampleModCount)
	{ 
		r_resolutionScale->value = strtof(r_resolutionScale->string, NULL);
		r_highQualityScaling->integer = atoi(r_highQualityScaling->string);

		if (r_highQualityScaling->integer & 1 && r_highQualityScaling->integer & 4)
		{
			if (r_resolutionScale->value < 1.0)
			{
				r_highQualityScaling->integer = 0;
			}
			else if (r_resolutionScale->value < 1.25)
			{
				r_resolutionScale->value = 1.0;
			}
			else if (r_resolutionScale->value < 1.37)
			{
				r_highQualityScaling->integer &= ~4;
			}
			else if (r_resolutionScale->value < 1.75)
			{
				r_resolutionScale->value = 1.5;
			}
			else if (r_resolutionScale->value <= 1.8)
			{
				r_highQualityScaling->integer &= ~4;
			}
			else if (r_resolutionScale->value < 2.35)
			{
				r_resolutionScale->value = 2.0;
			}
			else if (r_resolutionScale->value < 2.67)
			{
				r_highQualityScaling->integer &= ~4;
			}
			else
			{
				r_resolutionScale->value = 3.0;
			}
		}

		r_supersampleMode->integer = atoi(r_supersampleMode->string);
		if (r_resolutionScale->value > 1.0 && r_highQualityScaling->integer & 4 && r_supersampleMode->integer == 2)
		{
			r_supersampleMode->integer = 0;
		}

		/*if (r_supersample->modificationCount > 1 && r_supersample->modificationCount != supersampleModCount || r_supersampleSmoothness->modificationCount > 1 && supersampleSmoothenessModCount != r_supersampleSmoothness->modificationCount ||
			r_supersampleMultiframe->modificationCount > 1 && supersampleMultiframeModCount != r_supersampleMultiframe->modificationCount ||
			r_resolutionScale->modificationCount + r_fboMultisample->modificationCount > 2 && (r_resolutionScale->modificationCount != resScaleModCount || r_fboMultisample->modificationCount != fboMsModCount))*/
		{
			RB_SetupSupersampleBuffers();
			RB_SetupResolutionScaleBuffers();

			if (r_supersample->integer)
			{
				backEnd.textureLodBias = -(log(r_supersample->integer) / log(4.0) * r_supersampleLodFix->value);
			}
			else
			{
				backEnd.textureLodBias = 0.0;
			}
			if (r_resolutionScaleLodFix->value && r_resolutionScale->value > 1.f)
			{
				backEnd.textureLodBias += log(r_resolutionScale->value) * r_resolutionScaleLodFix->value;
			}
			for (i = 0 ; i < tr.numImages ; i++)
			{
				image_t *glt = tr.images[i];
				if (glt->mipmap)
				{
					GL_Bind(glt);
					qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, backEnd.textureLodBias);
				}
			}
		}

		backEnd.objects.resScaleWeightsCalculated = qfalse;

		resScaleModified = qfalse;
		resScaleModCount = r_resolutionScale->modificationCount;
		fboMsModCount = r_fboMultisample->modificationCount;
		scalingSampleRadius = r_scalingSampleRadius->modificationCount;
		HQScalingModCount = r_highQualityScaling->modificationCount;
		HQScalingModeModCount = r_highQualityScalingMode->modificationCount;
		supersampleModCount = r_supersample->modificationCount;
		supersampleSmoothenessModCount = r_supersampleSmoothness->modificationCount;
		supersampleMultiframeModCount = r_supersampleMultiframe->modificationCount;
		supersampleModeModCount = r_supersampleMode->modificationCount;
		resScaleLodFixModCount = r_resolutionScaleLodFix->modificationCount;
		simpleSupersampleModCount = r_simpleSupersample->modificationCount;
	}

	backEnd.viewParms.viewportWidth *= r_resolutionScale->value * (r_simpleSupersample->integer ? 2 : 1);
	backEnd.viewParms.viewportHeight *= r_resolutionScale->value * (r_simpleSupersample->integer ? 2 : 1);

	glBindFramebuffer(GL_FRAMEBUFFER, backEnd.objects.resScaleFramebuffersMultisample[0]);

	GLenum err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(err != GL_FRAMEBUFFER_COMPLETE)
		Com_Printf("RB_DrawSurfs: glCheckFramebufferStatus(GL_FRAMEBUFFER) == 0x%x\n", err);
}

void RB_SetGL2DWidthHeight(int width, int height)
{
	backEnd.projection2D = qtrue;

	qglViewport(0, 0, width, height);
	qglScissor(0, 0, width, height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, width, height, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();
	GL_State(GLS_DEPTHTEST_DISABLE);

	qglDisable(GL_CULL_FACE);
	qglDisable(GL_CLIP_PLANE0);
}

void RB_UtilizeOffScreenBuffers(void)
{
	int resScaleInt = 0;
	int i;

	/*if (glConfigExt.framebufferSrgbAvailable && r_srgbGammaCorrect->integer == 4)
	{
		glEnable(GL_FRAMEBUFFER_SRGB);
	}*/

	//glBindRenderbuffer(GL_RENDERBUFFER, backEnd.objects.resScaleColorBuffers[0]);
	//glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
	glBindFramebuffer(GL_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[0]);
	//glFramebufferRenderbuffer(GL_FRAMEBUFFER, drawBuffer, GL_RENDERBUFFER, backEnd.objects.resScaleColorBuffers[0]);
	BindTexture(backEnd.objects.resScaleTextures[0]);
	//glDrawBuffer(GL_COLOR_ATTACHMENT0);

	if (r_supersample->integer <= 1)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, backEnd.objects.resScaleFramebuffersMultisample[0]);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[0]);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		if (r_simpleSupersample->integer)
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[0]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[3]);
			glBlitFramebuffer(0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, 0, backEnd.viewParms.viewportWidth / 2, backEnd.viewParms.viewportHeight / 2, GL_COLOR_BUFFER_BIT, GL_LINEAR);
			backEnd.viewParms.viewportWidth /= 2;
			backEnd.viewParms.viewportHeight /= 2;

			glBindFramebuffer(GL_READ_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[3]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[0]);
			glBlitFramebuffer(0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight , GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
	}


	GLenum err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(err != GL_FRAMEBUFFER_COMPLETE)
		Com_Printf("RB_DrawSurfs: glCheckFramebufferStatus(GL_FRAMEBUFFER) == 0x%x\n", err);

	if (r_fboFxaa->integer & 2)
	{
		RB_PerformFxaa(backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, backEnd.objects.resScaleTextures[0], backEnd.objects.resScaleFramebuffers[0]);
	}

	if (r_resolutionScale->value != 1.f)
	{
		BindTexture(backEnd.objects.resScaleTextures[0]);
		glDisableClientState(GL_COLOR_ARRAY);


		if (r_highQualityScaling->integer & 2)
		{
			if (r_resolutionScale->integer >= 2 && r_resolutionScale->value / r_resolutionScale->integer == 1.f)
			{
				resScaleInt = r_resolutionScale->integer;
			}
			else if (r_resolutionScale->value > 2.f)
			{
				if (!(r_highQualityScaling->integer & (1 | 4)))
				{
					glBindFramebuffer(GL_READ_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[0]);
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[3]);
					glBlitFramebuffer(0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, 0, glConfig.vidWidth * r_resolutionScale->integer, glConfig.vidHeight * r_resolutionScale->integer, GL_COLOR_BUFFER_BIT, GL_LINEAR);
					backEnd.viewParms.viewportWidth = glConfig.vidWidth * r_resolutionScale->integer;
					backEnd.viewParms.viewportHeight = glConfig.vidHeight * r_resolutionScale->integer;
					resScaleInt = r_resolutionScale->integer;
				}
				else
				{
					resScaleInt = -(r_resolutionScale->integer);
				}
			}

			if (resScaleInt)
			{
				RB_SetGL2DWidthHeight(backEnd.viewParms.viewportWidth / r_resolutionScale->integer, backEnd.viewParms.viewportHeight / r_resolutionScale->integer);

				glBindFramebuffer(GL_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[1]);

				glUseProgram(backEnd.objects.resScaleProgramObjects[PO_MINIFY_AVERAGE]);
				glUniform1i(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_MINIFY_AVERAGE], "Texture"), 0);
				glUniform1f(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_MINIFY_AVERAGE], "ScaleInt"), r_resolutionScale->integer);
				glUniform2f(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_MINIFY_AVERAGE], "TextureSize"), backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

				/*glBindFramebuffer(GL_READ_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[0]);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[2]);
				glBlitFramebuffer(0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, 0, backEnd.viewParms.viewportWidth / r_resolutionScale->integer, backEnd.viewParms.viewportHeight / r_resolutionScale->integer, GL_COLOR_BUFFER_BIT, GL_LINEAR);*/

				backEnd.viewParms.viewportWidth /= r_resolutionScale->integer;
				backEnd.viewParms.viewportHeight /= r_resolutionScale->integer;

				DrawStuff(backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

				glBindFramebuffer(GL_READ_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[1]);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[2]);
				glBlitFramebuffer(0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

				BindTexture(backEnd.objects.resScaleTextures[1]);
			}
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		RB_SetGL2DWidthHeight(glConfig.vidWidth, glConfig.vidHeight);

		if (resScaleInt > 0)
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[2]);
			glBlitFramebuffer(0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		else if (!(r_highQualityScaling->integer & 1) /*|| r_resolutionScale->value < 1.f*/)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			DrawStuffDynamicRes(glConfig.vidWidth, glConfig.vidHeight, dynamicScale);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		else
		{
			float radius;
			float multiplier = r_scalingSampleRadiusMultiplier->value;
			static int reduced = 0;
			GLuint program;
			if (r_highQualityScaling->integer & 4)
			{
				//int cFrame = tr.frameCount % r_supersampleMultiframe->integer;
				int numJitters = min(max(r_supersample->integer, 1), 8);

				//program = backEnd.objects.resScaleProgramObjects[backEnd.objects.resScaleWeightsCalculated ? PO_RESOLUTION_SCALE_REF_PRE_CALCED_WEIGHTS : PO_RESOLUTION_SCALE_PRE_CALC_WEIGHTS];
				if (r_supersample->integer && r_supersampleMode->integer == 2)
				{
					program = backEnd.objects.resScaleProgramObjects[PO_RESOLUTION_SCALE_REF_PRE_CALCED_WEIGHTS_SUPERSAMPLE_RESOLVE];
				}
				else
				{
					program = backEnd.objects.resScaleProgramObjects[r_resolutionScale->value == r_resolutionScale->integer ? PO_RESOLUTION_SCALE_REF_PRE_CALCED_WEIGHTS : PO_RESOLUTION_SCALE_REF_PRE_CALCED_WEIGHTS_MULTI_PATTERN];
				}
				radius = r_scalingSampleRadius->value;
				if (!backEnd.objects.resScaleWeightsCalculated)
				{
					int numPatterns, i, j;
					if (r_resolutionScale->value - r_resolutionScale->integer > 0.f)
					{
						numPatterns = 1.f / (r_resolutionScale->value - r_resolutionScale->integer);
						numPatterns = min(numPatterns, MAX_SCALING_SAMPLE_PATTERNS);
					}
					else
					{
						numPatterns = 1;
					}
					//numPatterns = 1;
					RB_CalcScalingWeights(r_highQualityScalingMode->integer, radius, numPatterns, numJitters, backEnd.objects.supersampleJittersInPixel[0], &backEnd.objects.resScaleWeights);
					glUseProgram(program);
					glUniform1fv(glGetUniformLocation(program, "Weights"), ARRAY_LEN(backEnd.objects.resScaleWeights.buffer), backEnd.objects.resScaleWeights.buffer);
					/*glUniform2iv(glGetUniformLocation(program, "WeightsIndexes"), MAX_SCALING_SAMPLE_PATTERNS * MAX_SCALING_SAMPLE_PATTERNS, backEnd.objects.resScaleWeights.indexes);
					glUniform2iv(glGetUniformLocation(program, "WeightCount"), MAX_SCALING_SAMPLE_PATTERNS * MAX_SCALING_SAMPLE_PATTERNS, backEnd.objects.resScaleWeights.counter);
					glUniform2fv(glGetUniformLocation(program, "FirstPos"), MAX_SCALING_SAMPLE_PATTERNS * MAX_SCALING_SAMPLE_PATTERNS, backEnd.objects.resScaleWeights.FirstPos);*/
					if (program == backEnd.objects.resScaleProgramObjects[PO_RESOLUTION_SCALE_REF_PRE_CALCED_WEIGHTS_MULTI_PATTERN])
					{
#if USE_WEIGHT_BUFFER_TEXTURE
						glActiveTexture(GL_TEXTURE2);
						glBindTexture(GL_TEXTURE_1D, backEnd.objects.resScaleWeights.bufferTexture);
						glTexImage1D(GL_TEXTURE_1D, 0, GL_R8_SNORM, 400, 0, GL_RED, GL_FLOAT, backEnd.objects.resScaleWeights.buffer);
						glActiveTexture(GL_TEXTURE0);

						glUniform1i(glGetUniformLocation(program, "WeightBufferTexture"), 2);
#endif

						for (i = 0; i < numPatterns; i++)
						{
							for (j = 0; j < numPatterns; j++)
							{
								//glUniform1fv(glGetUniformLocation(program, va("Weights[%i][%i]", i, j)), ARRAY_LEN(backEnd.objects.resScaleWeights[i][j].buffer), backEnd.objects.resScaleWeights[i][j].buffer);
								glUniform2i(glGetUniformLocation(program, va("WeightCount[%i][%i]", i, j)), backEnd.objects.resScaleWeights.counter[i][j][0][0], backEnd.objects.resScaleWeights.counter[i][j][0][1]);
								glUniform2f(glGetUniformLocation(program, va("FirstPos[%i][%i]", i, j)), backEnd.objects.resScaleWeights.FirstPos[i][j][0][0], backEnd.objects.resScaleWeights.FirstPos[i][j][0][1]);
								glUniform1i(glGetUniformLocation(program, va("WeightsIndexes[%i][%i]", i, j)), backEnd.objects.resScaleWeights.indexes[i][j]);
							}
						}

						glUniform1i(glGetUniformLocation(program, "NumPatterns"), numPatterns);
					}
					else if (program == backEnd.objects.resScaleProgramObjects[PO_RESOLUTION_SCALE_REF_PRE_CALCED_WEIGHTS_SUPERSAMPLE_RESOLVE])
					{
						static GLint textureUnits[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
						for (i = 0; i < numJitters; i++)
						{
							glActiveTexture(GL_TEXTURE2 + i);
							//glClientActiveTexture(GL_TEXTURE0 + i);
							glBindTexture(GL_TEXTURE_2D, backEnd.objects.supersampleTextures[i]);
							//glDisableClientState(GL_COLOR_ARRAY);
						}
						glActiveTexture(GL_TEXTURE0 + glState.currenttmu);
						for (i = 0; i < numJitters; i++)
						{
							glUniform2i(glGetUniformLocation(program, va("WeightCount[%i]", i)), backEnd.objects.resScaleWeights.counter[0][0][i][0], backEnd.objects.resScaleWeights.counter[0][0][i][1]);
							glUniform2f(glGetUniformLocation(program, va("FirstPos[%i]", i)), backEnd.objects.resScaleWeights.FirstPos[0][0][i][0], backEnd.objects.resScaleWeights.FirstPos[0][0][i][1]);
						}
						glUniform1i(glGetUniformLocation(program, "NumJitters"), numJitters);
						glUniform1iv(glGetUniformLocation(program, "Textures"), numJitters, textureUnits);
					}
					else
					{
						glUniform2i(glGetUniformLocation(program, "WeightCount"), backEnd.objects.resScaleWeights.counter[0][0][0][0], backEnd.objects.resScaleWeights.counter[0][0][0][1]);
						glUniform2f(glGetUniformLocation(program, "FirstPos"), backEnd.objects.resScaleWeights.FirstPos[0][0][0][0], backEnd.objects.resScaleWeights.FirstPos[0][0][0][1]);
					}
				}
			}
			else if (r_highQualityScalingMode->integer == 1)
			{
				if (r_scalingSampleRadius->value == 2.0)
				{
					program = backEnd.objects.resScaleProgramObjects[PO_RESOLUTION_SCALE_LANCZOS2];
				}
				else if (r_scalingSampleRadius->value == 2.5)
				{
					program = backEnd.objects.resScaleProgramObjects[PO_RESOLUTION_SCALE_LANCZOS2_5];
				}
				else if (r_scalingSampleRadius->value == 3.0)
				{
					program = backEnd.objects.resScaleProgramObjects[PO_RESOLUTION_SCALE_LANCZOS3];
				}
				else
				{
					program = backEnd.objects.resScaleProgramObjects[PO_RESOLUTION_SCALE_MINIFY];
				}

				radius = r_scalingSampleRadius->value;
			}
			else if(r_highQualityScalingMode->integer == 2)
			{
				program = backEnd.objects.resScaleProgramObjects[PO_RESOLUTION_SCALE_MINIFY];
				radius = 1.f;
			}
			else
			{
				program = backEnd.objects.resScaleProgramObjects[PO_RESOLUTION_SCALE_BICUBIC];
				radius = 2.f;
			}
			glUseProgram(program);

			if (!backEnd.objects.resScaleWeightsCalculated)
			{
				R_GetGLError();
				glUniform1i(glGetUniformLocation(program, "Texture"), 0);
				//glUniform2f(glGetUniformLocation(program, "TextureSize"), backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
				//glUniform2f(glGetUniformLocation(program, "OutputSize"), glConfig.vidWidth, glConfig.vidHeight);
				glUniform2f(glGetUniformLocation(program, "InputPixelSize"), 1.f / backEnd.viewParms.viewportWidth, 1.f / backEnd.viewParms.viewportHeight);
				glUniform2f(glGetUniformLocation(program, "OutputPixelSize"), 1.f / glConfig.vidWidth, 1.f / glConfig.vidHeight);
				glUniform1i(glGetUniformLocation(program, "Mode"), r_highQualityScalingMode->integer);
				glUniform1f(glGetUniformLocation(program, "Radius"), radius);
				glUniform1f(glGetUniformLocation(program, "RadiusMultiplier"), multiplier);
				glUniform1f(glGetUniformLocation(program, "ScaleFactor"), dynamicScale);
				R_GetGLError();
			}

			if (program == backEnd.objects.resScaleProgramObjects[PO_RESOLUTION_SCALE_REF_PRE_CALCED_WEIGHTS_MULTI_PATTERN])
			{
				DrawStuff_GL430();
			}
			else
			{
				DrawStuffDynamicRes(glConfig.vidWidth, glConfig.vidHeight, dynamicScale);
			}

			backEnd.objects.resScaleWeightsCalculated = qtrue;
		}
	}
	else
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[0]);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glDrawBuffer(GL_BACK);
		glBlitFramebuffer(0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}

	if (r_fboFxaa->integer & 1)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backEnd.objects.windowSizeFramebuffer);
		glReadBuffer(GL_BACK);
		glBlitFramebuffer(0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		RB_PerformFxaa(glConfig.vidWidth, glConfig.vidHeight, backEnd.objects.windowSizeTexture, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	//glBindFramebuffer(GL_READ_FRAMEBUFFER, backEnd.objects.resScaleFramebuffersMultisample[0]);
	//glBlitFramebuffer(0, 0, glConfig.vidWidth * r_resolutionScale->value, glConfig.vidHeight * r_resolutionScale->value, 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_BUFFER_BIT, GL_NEAREST);

	R_GetGLError();

	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

#define USE_FBO ((r_fboMultisample->integer || r_resolutionScale->value != 1.f || r_supersample->integer) && backEnd.refdef.y == 0)
/**
* @brief RB_DrawSurfs
* @param[in] data
* @return
*/
void *RB_DrawSurfs(const void *data)
{
	const drawSurfsCommand_t *cmd;
	int i;
	GLenum err;

	// finish any 2D drawing if needed
	if (tess.numIndexes)
	{
		RB_EndSurface();
	}

	cmd = ( const drawSurfsCommand_t * ) data;

	backEnd.refdef    = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	if (USE_FBO)
	{
		RB_PrepairOffScreenBuffers();
	}

	/*if (glConfigExt.framebufferSrgbAvailable)
	{
	glEnable(GL_FRAMEBUFFER_SRGB);
	}
	else
	{
	glDisable(GL_FRAMEBUFFER_SRGB);
	}*/

	if (r_supersample->integer && backEnd.refdef.y == 0)
	{
		static int cFrame = 0;
		mat4_t orgProjectionMatrix, orgModelMatrix, orgWorldModelMatrix;
		int i;
		int renderBeginTime = 0, renderEndTime = 0, firstFrameBeginTime = 0;
		float jitters[MAX_SUPERSAMPLE_SAMPLES][2]; //, jittersInPixel[MAX_SUPERSAMPLE_SAMPLES][2];
		int samples = max(1, r_supersample->integer);

		mat4_copy(backEnd.viewParms.projectionMatrix, orgProjectionMatrix);
		for (i = 0; i < samples; i++)
		{
			mat4_t ssaa;
			mat4_reset_translate(ssaa, (backEnd.objects.supersampleOffsets[cFrame][i][0] * 2 - 1) * r_supersampleSmoothness->value / backEnd.viewParms.viewportWidth, (backEnd.objects.supersampleOffsets[cFrame][i][1] * 2 - 1) * r_supersampleSmoothness->value / backEnd.viewParms.viewportHeight, 0);
			mat4_mult(ssaa, orgProjectionMatrix, backEnd.viewParms.projectionMatrix);

			RB_RenderDrawSurfList(cmd->drawSurfs, cmd->numDrawSurfs);

			//if (USE_FBO)
			if (1)
			{
				/*if (glConfigExt.framebufferSrgbAvailable)
				{
				glEnable(GL_FRAMEBUFFER_SRGB);
				}*/

				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backEnd.objects.supersampleFramebuffers[i]);
				glBlitFramebuffer(0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
				glBindFramebuffer(GL_FRAMEBUFFER, backEnd.objects.resScaleFramebuffersMultisample[0]);
			}
			else
			{
				glAccum(i ? GL_ACCUM : GL_LOAD, 1.f / r_supersample->integer);
			}
		}
		//if (USE_FBO)
		if (r_resolutionScale->value == 1.f || r_supersampleMode->integer != 2)
		{
			static GLint textureUnits[] = {0, 1 ,2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};

			for (i = 0; i < samples; i++)
			{
				glActiveTexture(GL_TEXTURE0 + i);
				//glClientActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, backEnd.objects.supersampleTextures[i]);
				//glDisableClientState(GL_COLOR_ARRAY);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, backEnd.objects.resScaleFramebuffers[0]);
			RB_SetGL2DWidthHeight(backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
			glUseProgram(backEnd.objects.resScaleProgramObjects[PO_SUPERSAMPLE_RESOLVE]);
			glUniform1iv(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_SUPERSAMPLE_RESOLVE], "Textures"), samples, textureUnits);
			glUniform1i(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_SUPERSAMPLE_RESOLVE], "NumTextures"), samples);
			glUniform1i(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_SUPERSAMPLE_RESOLVE], "Mode"), r_supersampleMode->integer);
			//glUniform1i(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_SUPERSAMPLE_RESOLVE], "Texture"), textureUnits[0]);
			glUniform2f(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_SUPERSAMPLE_RESOLVE], "TextureSize"), backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
			glUniform2f(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_SUPERSAMPLE_RESOLVE], "OutputSize"), backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
			glUniform1f(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_SUPERSAMPLE_RESOLVE], "Radius"), r_scalingSampleRadius->value);
			switch (r_supersampleMode->integer)
			{
			case 2:
				glUniform1fv(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_SUPERSAMPLE_RESOLVE], "Weights"), MAX_SUPERSAMPLE_SAMPLES * 36, backEnd.objects.supersampleWeights[cFrame]);
				glUniform2fv(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_SUPERSAMPLE_RESOLVE], "Jitters"), samples, backEnd.objects.supersampleJittersInPixel);
				break;
			case 1:
				for (i = 0; i < r_supersample->integer; i++)
				{
					jitters[i][0] = backEnd.objects.supersampleJittersInPixel[cFrame][i][0] / backEnd.viewParms.viewportWidth;
					jitters[i][1] = backEnd.objects.supersampleJittersInPixel[cFrame][i][1] / backEnd.viewParms.viewportHeight;
				}
				glUniform2fv(glGetUniformLocation(backEnd.objects.resScaleProgramObjects[PO_SUPERSAMPLE_RESOLVE], "Jitters"), samples, jitters);
				break;
			}
			DrawStuff(backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
			glUseProgram(0);
			glActiveTexture(GL_TEXTURE0 + glState.currenttmu);
			//glClientActiveTexture(GL_TEXTURE0 + glState.currenttmu);
		}

		cFrame++;
		if (cFrame >= r_supersampleMultiframe->integer)
		{
			cFrame = 0;
		}
	}
	else
	{
		RB_RenderDrawSurfList(cmd->drawSurfs, cmd->numDrawSurfs);
	}

	// draw sun
	RB_DrawSun();

	// darken down any stencil shadows
	RB_ShadowFinish();

	// add light flares on lights that aren't obscured
	RB_RenderFlares();

	if (USE_FBO)
	{
		RB_UtilizeOffScreenBuffers();
	}

	//glDisable(GL_FRAMEBUFFER_SRGB);

	return ( const void * ) (cmd + 1);
}

/**
 * @brief RB_DrawBuffer
 * @param[in] data
 * @return
 */
const void *RB_DrawBuffer(const void *data)
{
	const drawBufferCommand_t *cmd = ( const drawBufferCommand_t * ) data;

	qglDrawBuffer(cmd->buffer);

	// clear screen for debugging
	if (r_clear->integer)
	{
		qglClearColor(1, 0, 0.5, 1);
		qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	return ( const void * ) (cmd + 1);
}

/**
 * @brief RB_GammaScreen
 */
void RB_GammaScreen(void)
{
	// We force the 2D drawing
	RB_SetGL2D();
	R_ScreenGamma();
}

/**
 * @brief Draw all the images to the screen, on top of whatever
 * was there.  This is used to test for texture thrashing.
 *
 * Also called by RE_EndRegistration
 */
void RB_ShowImages(void)
{
	int     i;
	image_t *image;
	float   x, y, w, h;
	int     start, end;

	if (!backEnd.projection2D)
	{
		RB_SetGL2D();
	}

	qglClear(GL_COLOR_BUFFER_BIT);

	qglFinish();

	start = ri.Milliseconds();

	for (i = 0 ; i < tr.numImages ; i++)
	{
		image = tr.images[i];

		w = glConfig.vidWidth / 40;
		h = glConfig.vidHeight / 30;

		x = i % 40 * w;
		y = i / 30 * h;

		// show in proportional size in mode 2
		if (r_showImages->integer == 2)
		{
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_Bind(image);
		qglBegin(GL_QUADS);
		qglTexCoord2f(0, 0);
		qglVertex2f(x, y);
		qglTexCoord2f(1, 0);
		qglVertex2f(x + w, y);
		qglTexCoord2f(1, 1);
		qglVertex2f(x + w, y + h);
		qglTexCoord2f(0, 1);
		qglVertex2f(x, y + h);
		qglEnd();
	}

	qglFinish();

	end = ri.Milliseconds();
	Ren_Print("%i msec to draw all images\n", end - start);
}

/*
 * @brief RB_DrawBounds
 * @param[in,out] mins
 * @param[in,out] maxs
 *
 * @note Unused.
void RB_DrawBounds(vec3_t mins, vec3_t maxs)
{
    vec3_t center;

    GL_Bind(tr.whiteImage);
    GL_State(GLS_POLYMODE_LINE);

    // box corners
    qglBegin(GL_LINES);
    qglColor3f(1, 1, 1);

    qglVertex3f(mins[0], mins[1], mins[2]);
    qglVertex3f(maxs[0], mins[1], mins[2]);
    qglVertex3f(mins[0], mins[1], mins[2]);
    qglVertex3f(mins[0], maxs[1], mins[2]);
    qglVertex3f(mins[0], mins[1], mins[2]);
    qglVertex3f(mins[0], mins[1], maxs[2]);

    qglVertex3f(maxs[0], maxs[1], maxs[2]);
    qglVertex3f(mins[0], maxs[1], maxs[2]);
    qglVertex3f(maxs[0], maxs[1], maxs[2]);
    qglVertex3f(maxs[0], mins[1], maxs[2]);
    qglVertex3f(maxs[0], maxs[1], maxs[2]);
    qglVertex3f(maxs[0], maxs[1], mins[2]);
    qglEnd();

    center[0] = (mins[0] + maxs[0]) * 0.5f;
    center[1] = (mins[1] + maxs[1]) * 0.5f;
    center[2] = (mins[2] + maxs[2]) * 0.5f;

    // center axis
    qglBegin(GL_LINES);
    qglColor3f(1, 0.85f, 0);

    qglVertex3f(mins[0], center[1], center[2]);
    qglVertex3f(maxs[0], center[1], center[2]);
    qglVertex3f(center[0], mins[1], center[2]);
    qglVertex3f(center[0], maxs[1], center[2]);
    qglVertex3f(center[0], center[1], mins[2]);
    qglVertex3f(center[0], center[1], maxs[2]);
    qglEnd();
}
*/

/**
 * @brief RB_SwapBuffers
 * @param[in] data
 * @return
 */
const void *RB_SwapBuffers(const void *data)
{
	const swapBuffersCommand_t *cmd;

	// finish any 2D drawing if needed
	if (tess.numIndexes)
	{
		RB_EndSurface();
	}

	// texture swapping test
	if (r_showImages->integer)
	{
		RB_ShowImages();
	}

	RB_GammaScreen();

	cmd = ( const swapBuffersCommand_t * ) data;

	// we measure overdraw by reading back the stencil buffer and
	// counting up the number of increments that have happened
	if (r_measureOverdraw->integer)
	{
		int           i;
		long          sum = 0;
		unsigned char *stencilReadback;

		stencilReadback = ri.Hunk_AllocateTempMemory(glConfig.vidWidth * glConfig.vidHeight);
		qglReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback);

		for (i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++)
		{
			sum += stencilReadback[i];
		}

		backEnd.pc.c_overDraw += sum;
		ri.Hunk_FreeTempMemory(stencilReadback);
	}

	if (!glState.finishCalled)
	{
		qglFinish();
	}

	Ren_LogComment("***************** RB_SwapBuffers *****************\n\n\n");

	ri.GLimp_SwapFrame();

	backEnd.projection2D = qfalse;

	return ( const void * ) (cmd + 1);
}

/**
 * @brief RB_RenderToTexture
 * @param[in] data
 * @return
 */
const void *RB_RenderToTexture(const void *data)
{
	const renderToTextureCommand_t *cmd = ( const renderToTextureCommand_t * ) data;

	//ri.Printf( PRINT_ALL, "RB_RenderToTexture\n" );

	GL_Bind(cmd->image);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, cmd->x, cmd->y, cmd->w, cmd->h, 0);
	//qglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cmd->x, cmd->y, cmd->w, cmd->h );

	return ( const void * ) (cmd + 1);
}

/**
 * @brief RB_Finish
 * @param[in] data
 * @return
 */
const void *RB_Finish(const void *data)
{
	const renderFinishCommand_t *cmd = ( const renderFinishCommand_t * ) data;

	//ri.Printf( PRINT_ALL, "RB_Finish\n" );

	qglFinish();

	return ( const void * ) (cmd + 1);
}

/**
 * @brief RB_ExecuteRenderCommands
 * @param[in] data
 */
void RB_ExecuteRenderCommands(const void *data)
{
	int t1, t2;

	t1 = ri.Milliseconds();

	while (1)
	{
		switch (*( const int * ) data)
		{
		case RC_SET_COLOR:
			data = RB_SetColor(data);
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic(data);
			break;
		case RC_2DPOLYS:
			data = RB_Draw2dPolys(data);
			break;
		case RC_ROTATED_PIC:
			data = RB_RotatedPic(data);
			break;
		case RC_STRETCH_PIC_GRADIENT:
			data = RB_StretchPicGradient(data);
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs(data);
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer(data);
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers(data);
			break;
		case RC_SCREENSHOT:
			data = RB_TakeScreenshotCmd(data);
			break;
		case RC_VIDEOFRAME:
			data = RB_TakeVideoFrameCmd(data);
			break;
		case RC_RENDERTOTEXTURE:
			data = RB_RenderToTexture(data);
			break;
		case RC_FINISH:
			data = RB_Finish(data);
			break;
		case RC_END_OF_LIST:
		default:
			// stop rendering on this thread
			t2              = ri.Milliseconds();
			backEnd.pc.msec = t2 - t1;
			return;
		}
	}
}
