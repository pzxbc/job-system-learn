#ifndef DEBUGGRAPHICSLRB_H
#define DEBUGGRAPHICSLRB_H

#define ENABLE_DEBUG_GRAPHICS

const int NUM_DEBUG_FRAMES = 4;
const int MAX_DEBUG_GRAPHICS_TIME_ENTRIES = 8192;

#ifdef ENABLE_DEBUG_GRAPHICS
void DrawSolidDebugBox( float startX, float startY, float endX, float endY, UINT color );
void DrawDebugGraphics();
int  StartDebugMeter( UINT framePingPong, UINT color = 0x80FF00FF, char *pName="NAME ME" );
void EndDebugMeter( UINT framePingPong,  int index );
void ResetDebugGraphics();
void DestroyDebugGraphics();
#else
#define DrawSolidDebugBox( a,b,c,d,e,f,g )
#define DrawDebugGraphics()
#define StartDebugMeter( a ) 0
#define EndDebugMeter( a )
#define PrintDebugMeter( a )
#define ResetDebugGraphics()
#define DestroyDebugGraphics()
#endif

#endif // DEBUGGRAPHICSLRB_H