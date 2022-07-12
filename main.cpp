#include <iostream>
#include <windows.h>
#include <assert.h>
#include <GL/gl.h>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#include "defines.h"
#include "math.hpp"
#include "opengl.hpp"

static u32 FirsLineIndexOnScreen = 0;
#include "utils.hpp"
#include "input.hpp"

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600

static bool Is_Running = true;
HDC Device_Context;

#define MAX_CLIPBOARD_BUFFER 1000
static char Clipboard[MAX_CLIPBOARD_BUFFER];
static s32 TextSize = 0;
static s32 ClipboardSize = 0;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
    switch (uMsg) 
    {
    case WM_CREATE: 
    {
        InitializeOpenGL(hwnd);
        return 0;
    } break;
    case WM_SIZE: 
    {
        const u32 Width  = LOWORD(lParam);
        const u32 Height = HIWORD(lParam);
        glViewport(0, 0, (GLint)Width, (GLint)Height);
    } break;
    case WM_DESTROY: 
    {
        Is_Running = false;
    } break;
    case WM_CHAR:
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    } 
    return 0; 
}

s32 WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) 
{
    Assert(AllocConsole());

    HWND window_handle = CreateOpenGLWindow(hInstance, nCmdShow,
                                            WINDOW_WIDTH, WINDOW_HEIGHT);
    Device_Context = GetDC(window_handle);

    CreateFontTextureResult TextureData = CreateFontTexture();
    v2* TextureLookupTable = CreateFontLookupTable(TextureData);
    
    // Define orthographic projection
    f32 Left  = 0;
    f32 Right = WINDOW_WIDTH;
    f32 Top   = WINDOW_HEIGHT;
    f32 Bottom = 0;
    f32 Near = 0;
    f32 Far = 5;
    
    m4 OrthoMatrix = MakeOrthoMatrix(Left, Right, Top, Bottom, Near, Far);

    f32 CharAspectRatio = 16.0f/30.0f;
    v3 Scale = v3(12, 30, 1);
    Scale.y = Scale.x / CharAspectRatio;
    v3 TextColor = v3(51, 82, 59) / 200.0f;

    GLuint TextShader   = LoadShaderFromFiles("../shaders/vert.shader", "../shaders/frag.shader");
    GLuint CursorShader = LoadShaderFromFiles("../shaders/cursor_vertex.shader", "../shaders/cursor_frag.shader");

    GLuint TextVAO, TextVBO;
    glGenVertexArrays(1, &TextVAO);
    glBindVertexArray(TextVAO);
    glGenBuffers(1, &TextVBO);
    glBindBuffer(GL_ARRAY_BUFFER, TextVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)sizeof(v3));
    glVertexAttribIPointer(2, 1, GL_INT, sizeof(Vertex), (void*)(sizeof(v3)*2));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    u32 CursorVAO, CursorVBO;
    glGenVertexArrays(1, &CursorVAO);
    glBindVertexArray(CursorVAO);
    glGenBuffers(1, &CursorVBO);
    glBindBuffer(GL_ARRAY_BUFFER, CursorVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CursorVertex), 0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CursorVertex), (void*)sizeof(v3));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(CursorVertex), (void*)(sizeof(v3) + sizeof(v4)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glUseProgram(TextShader);
    s32 OrthoMatrixLocation = glGetUniformLocation(TextShader, "OrthoMatrix");
    s32 OrthoMatrixLocationCursor = glGetUniformLocation(CursorShader, "OrthoMatrix");
    s32 TextureLookupTableLocation = glGetUniformLocation(TextShader, "TextureLookupTable");
    glUniformMatrix4fv(OrthoMatrixLocation, 1, GL_TRUE, (f32*)&OrthoMatrix);
    glUniform2fv(TextureLookupTableLocation, 94 * 4, (f32*)TextureLookupTable);

    glUseProgram(CursorShader);
    glUniformMatrix4fv(OrthoMatrixLocationCursor, 1, GL_TRUE, (f32*)&OrthoMatrix);

    WarnIfNot(OrthoMatrixLocation >= 0);
    WarnIfNot(OrthoMatrixLocationCursor >= 0);
    WarnIfNot(TextureLookupTableLocation >= 0);
    FreeMemory(TextureLookupTable);

    FrameArena Arena = FrameArenaCreate(Megabytes(2));
    
    TextBox Box = {WINDOW_WIDTH, WINDOW_HEIGHT, Scale.x*2, Scale.y*2};
    
    char* FillText = "Test text thomg\n\n\nldsaoflasdfoasd f\nint main() {\nreturn 0;\n} TestText1\nTestText2\nTestText3\nTestText4\nTestText5\nTestText6\nTestText7\nTestText8\nTestText9\nTestText10";
    SplitBuffer SB = SplitBufferCreate(1024, FillText, (u32)strlen(FillText));
    CalculateLinesResult Lines = CalculateLinesSB(Box, &Arena, SB);

    u32 PrimaryCursorPos = 0;
    u32 SecondaryCursorPos = 0;
    bool IsCursorMoved = false;

    InputHandle Input;
    MSG M;
    while(Is_Running && GetMessage(&M, window_handle, 0,0))
    {
        ProcessInputWin32(&Input,M);
        
        if(GetKeyDown(Input, KeyCode_Escape))
        {
            Is_Running = false;
        }

        if(M.message == WM_CHAR && M.wParam != VK_BACK) 
        {
            u8 Char = (u8)M.wParam;
            u32 CursorAdvanceAmount = 1;

            if(IsCursorMoved)
            {
                SplitBufferSetCursor(SB, PrimaryCursorPos);
                IsCursorMoved = false;
            }

            if (Char == '\n' || Char == '\r')
            {
                Char = '\n';
            }
            
            if(Char == '\t') // so only spaces supported lol
            {
                SplitBufferAddChar(SB, ' ');
                SplitBufferAddChar(SB, ' ');
                SplitBufferAddChar(SB, ' ');
                SplitBufferAddChar(SB, ' ');
                CursorAdvanceAmount = 4;
            }
            else
            {
                SplitBufferAddChar(SB, Char);
            }
            
            if(SecondaryCursorPos > PrimaryCursorPos)
            {
                SecondaryCursorPos += CursorAdvanceAmount;
            }
            PrimaryCursorPos += CursorAdvanceAmount;
        }
        
        if(GetKey(Input, KeyCode_Up))
        {
            v2 FakeScreenPos = CursorTextToScreen(&Lines, PrimaryCursorPos);
            u32 NewY = Clamp(TruncateF32ToS32(FakeScreenPos.y - 1), 0, Max(Lines.Count - 1, 0));
            u32 NewX = Clamp(TruncateF32ToS32(FakeScreenPos.x), 0, Lines.Lines[NewY].EndIndex - Lines.Lines[NewY].StartIndex);
            PrimaryCursorPos = Lines.Lines[NewY].StartIndex + NewX;
            IsCursorMoved = true;
        }
        else if(GetKey(Input, KeyCode_Down))
        {
            v2 FakeScreenPos = CursorTextToScreen(&Lines, PrimaryCursorPos);
            u32 NewY = Clamp(TruncateF32ToS32(FakeScreenPos.y + 1), 0, Max(Lines.Count - 1, 0));
            u32 NewX = Clamp(TruncateF32ToS32(FakeScreenPos.x), 0, Lines.Lines[NewY].EndIndex - Lines.Lines[NewY].StartIndex);
            PrimaryCursorPos = Lines.Lines[NewY].StartIndex + NewX;
            IsCursorMoved = true;
        }
        else if(GetKey(Input, KeyCode_Left))
        {
            PrimaryCursorPos = Clamp(PrimaryCursorPos -= 1, 0, SB.TextSize);
            IsCursorMoved = true;
        }
        else if(GetKey(Input, KeyCode_Right))
        {
            PrimaryCursorPos = Clamp(PrimaryCursorPos += 1, 0, SB.TextSize);            
            IsCursorMoved = true;
        }
        else if(GetKey(Input, KeyCode_Delete))
        {
            if(IsCursorMoved)
            {
                SplitBufferSetCursor(SB, PrimaryCursorPos);
            }

            SplitBufferRemoveCharDeleteKey(SB);

            if(SecondaryCursorPos > PrimaryCursorPos) // If secondary cursor position is ahead of primary move it's position one left 
            {
                SecondaryCursorPos -= 1;
            }
        }
        else if(GetKey(Input, KeyCode_Backspace))
        {
            if(IsCursorMoved)
            {
                SplitBufferSetCursor(SB, PrimaryCursorPos);
            }
            
            SplitBufferRemoveCharBackKey(SB);
            PrimaryCursorPos -= 1; // Cursor Move Left
            if(SecondaryCursorPos > PrimaryCursorPos) // If secondary cursor position is ahead of primary move it's position one left 
            {
                SecondaryCursorPos -= 1;
            }
        }

        if(GetKey(Input, KeyCode_Ctrl))
        {
            if(GetKeyDown(Input, KeyCode_C))
            {
                DebugLog("Copy\n");
                u32 CopyTextSize = 0;

                if(PrimaryCursorPos <= SB.Middle && SecondaryCursorPos <= SB.Middle) // Both in first segment
                {
                    if(PrimaryCursorPos > SecondaryCursorPos) // Primary is ahead of Secondary
                    {
                        CopyTextSize = PrimaryCursorPos - SecondaryCursorPos;
                        Memcpy(SB.Start + SecondaryCursorPos, Clipboard, CopyTextSize);
                    }
                    else // Secondary is ahead of Primary
                    {
                        CopyTextSize = SecondaryCursorPos - PrimaryCursorPos;
                        Memcpy(SB.Start + PrimaryCursorPos, Clipboard, CopyTextSize);
                    }
                }
                else if (PrimaryCursorPos >= SB.Middle && SecondaryCursorPos <= SB.Middle) // Primary is in second, Secondary is in first
                {
                    CopyTextSize = SB.Middle - SecondaryCursorPos;
                    Memcpy(SB.Start + SecondaryCursorPos, Clipboard, CopyTextSize);
                    Memcpy(SB.Start + SB.Second, Clipboard + CopyTextSize, (PrimaryCursorPos + (SB.Second - SB.Middle)) - SB.Second);
                    CopyTextSize += (PrimaryCursorPos + (SB.Second - SB.Middle)) - SB.Second;
                }
                else if (PrimaryCursorPos <= SB.Middle && SecondaryCursorPos >= SB.Middle) // Primary is in first, Secondary is in second
                {
                    CopyTextSize = SB.Middle - PrimaryCursorPos;
                    Memcpy(SB.Start + PrimaryCursorPos, Clipboard, CopyTextSize);
                    Memcpy(SB.Start + SB.Second, Clipboard + CopyTextSize, (SecondaryCursorPos + (SB.Second - SB.Middle)) - SB.Second);
                    CopyTextSize += (SecondaryCursorPos + (SB.Second - SB.Middle)) - SB.Second;
                }
                else if (PrimaryCursorPos >= SB.Middle && SecondaryCursorPos >= SB.Middle) // Both in second segment
                {
                    u32 Primary   = (PrimaryCursorPos + (SB.Second - SB.Middle));
                    u32 Secondary = (SecondaryCursorPos + (SB.Second - SB.Middle));
                    
                    if(Primary > Secondary) // Primary is ahead of Secondary
                    {
                        CopyTextSize = Primary - Secondary;
                        Memcpy(SB.Start + Secondary, Clipboard, CopyTextSize);
                    }
                    else // Secondary is ahead of Primary
                    {
                        CopyTextSize = Secondary - Primary;
                        Memcpy(SB.Start + Primary, Clipboard, CopyTextSize);
                    }
                }
                ClipboardSize = CopyTextSize;
                Clipboard[ClipboardSize] = 0;
            }
            else if(GetKeyDown(Input, KeyCode_Y))
            {
                DebugLog("Paste\n");

                SecondaryCursorPos = PrimaryCursorPos;
                SplitBufferSetCursor(SB, SecondaryCursorPos);
                Memcpy(Clipboard, SB.Start + SB.Middle, ClipboardSize);
                SB.Middle += ClipboardSize; // TODO: Handle buffer collisions
                SB.TextSize += ClipboardSize; // TODO: Handle buffer collisions
                
                Lines = CalculateLinesSB(Box, &Arena, SB);
                PrimaryCursorPos = SB.Middle;
            }
            else if(GetKeyDown(Input, KeyCode_W))
            {
                // Cutting is same as coping but you just delete after
                DebugLog("Cut\n");
                
                u32 CopyTextSize = 0;

                if(PrimaryCursorPos <= SB.Middle && SecondaryCursorPos <= SB.Middle) // Both in first segment
                {
                    if(PrimaryCursorPos > SecondaryCursorPos) // Primary is ahead of Secondary
                    {
                        CopyTextSize = PrimaryCursorPos - SecondaryCursorPos;
                        Memcpy(SB.Start + SecondaryCursorPos, Clipboard, CopyTextSize);
                    }
                    else // Secondary is ahead of Primary
                    {
                        CopyTextSize = SecondaryCursorPos - PrimaryCursorPos;
                        Memcpy(SB.Start + PrimaryCursorPos, Clipboard, CopyTextSize);
                    }
                }
                else if (PrimaryCursorPos >= SB.Middle && SecondaryCursorPos <= SB.Middle) // Primary is in second, Secondary is in first
                {
                    CopyTextSize = SB.Middle - SecondaryCursorPos;
                    Memcpy(SB.Start + SecondaryCursorPos, Clipboard, CopyTextSize);
                    Memcpy(SB.Start + SB.Second, Clipboard + CopyTextSize, (PrimaryCursorPos + (SB.Second - SB.Middle)) - SB.Second);
                    CopyTextSize += (PrimaryCursorPos + (SB.Second - SB.Middle)) - SB.Second;
                }
                else if (PrimaryCursorPos <= SB.Middle && SecondaryCursorPos >= SB.Middle) // Primary is in first, Secondary is in second
                {
                    CopyTextSize = SB.Middle - PrimaryCursorPos;
                    Memcpy(SB.Start + PrimaryCursorPos, Clipboard, CopyTextSize);
                    Memcpy(SB.Start + SB.Second, Clipboard + CopyTextSize, (SecondaryCursorPos + (SB.Second - SB.Middle)) - SB.Second);
                    CopyTextSize += (SecondaryCursorPos + (SB.Second - SB.Middle)) - SB.Second;
                }
                else if (PrimaryCursorPos >= SB.Middle && SecondaryCursorPos >= SB.Middle) // Both in second segment
                {
                    u32 Primary   = (PrimaryCursorPos + (SB.Second - SB.Middle));
                    u32 Secondary = (SecondaryCursorPos + (SB.Second - SB.Middle));
                    
                    if(Primary > Secondary) // Primary is ahead of Secondary
                    {
                        CopyTextSize = Primary - Secondary;
                        Memcpy(SB.Start + Secondary, Clipboard, CopyTextSize);
                    }
                    else // Secondary is ahead of Primary
                    {
                        CopyTextSize = Secondary - Primary;
                        Memcpy(SB.Start + Primary, Clipboard, CopyTextSize);
                    }
                }
                ClipboardSize = CopyTextSize;
                Clipboard[ClipboardSize] = 0;

                SplitBufferSetCursor(SB, PrimaryCursorPos);
                if(PrimaryCursorPos < SecondaryCursorPos) // Delete forwards
                {
                    for(s32 I = 0; I < ClipboardSize; ++I)
                    {
                        SplitBufferRemoveCharDeleteKey(SB);
                    }
                }
                else // Delete backwards
                {
                    for(s32 I = 0; I < ClipboardSize; ++I)
                    {
                        SplitBufferRemoveCharBackKey(SB);
                    }
                }

                // update screen poition && secondary cursor position
                SecondaryCursorPos = SB.Middle;
                PrimaryCursorPos = SB.Middle;
            }
            else if(GetKeyDown(Input, KeyCode_Space))
            {
                SecondaryCursorPos = PrimaryCursorPos;
            }
        }

        if(GetKeyDown(Input, KeyCode_Home))
        {
            DebugLog("Home Key\n");
            PrimaryCursorPos = Lines.Lines[CursorGetCurrentLine(&Lines, PrimaryCursorPos)].StartIndex;
            IsCursorMoved = true;
        }
        if(GetKeyDown(Input, KeyCode_End))
        {
            DebugLog("End Key\n");
            PrimaryCursorPos = Lines.Lines[CursorGetCurrentLine(&Lines, PrimaryCursorPos)].EndIndex;
            IsCursorMoved = true;
        }

        if(GetKey(Input, KeyCode_Alt) && GetKeyDown(Input, KeyCode_W))
        {
            DebugLog("Alt Copy\n");
        }

        if(GetKey(Input, KeyCode_Ctrl) && GetKeyDown(Input, KeyCode_P))
        {
            DebugLog("CTRL PLUS\n");
            Scale.x += 1.5f;
            Scale.y = Scale.x / CharAspectRatio;
            Box = {WINDOW_WIDTH, WINDOW_HEIGHT, Scale.x*2, Scale.y*2};
        }
        if(GetKey(Input, KeyCode_Ctrl) && GetKeyDown(Input, KeyCode_M))
        {
            DebugLog("CTRL MINUS\n");
            Scale.x -= 1.5f;
            Scale.y = Scale.x / CharAspectRatio;
            Box = {WINDOW_WIDTH, WINDOW_HEIGHT, Scale.x*2, Scale.y*2};
        }

        if(IsCursorMoved)
        {
            v2 CursorScreenPosition = CursorTextToScreen(&Lines, PrimaryCursorPos);
            DebugLog("CursorPosition : %i, X:%i Y:%i\n", PrimaryCursorPos, u32(CursorScreenPosition.x), u32(CursorScreenPosition.y));
        }
        
        glClearColor(30.0f/255.0f,30.0f/255.0f,30.0f/255.0f,30.0f/255.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        Lines = CalculateLinesSB(Box, &Arena, SB);

        u32 CursorCurrentLineIndex = CursorGetCurrentLine(&Lines, PrimaryCursorPos);

        static u32 OldPage = 0;
        u32 Page = CursorCurrentLineIndex / Lines.MaxLinesOnScreen;
        if(Page != OldPage)
        {
            DebugLog("Scroll\n");
            f32 AdvanceSize = Lines.MaxLinesOnScreen * Box.CharacterHeight;
            OrthoMatrix = MakeOrthoMatrix(Left, Right, Top - AdvanceSize*Page, Bottom - AdvanceSize*Page,
                                          Near, Far);
            OldPage = Page;
            glUseProgram(TextShader);
            glUniformMatrix4fv(OrthoMatrixLocation, 1, GL_TRUE, (f32*)&OrthoMatrix);
            glUseProgram(CursorShader);
            glUniformMatrix4fv(OrthoMatrixLocationCursor, 1, GL_TRUE, (f32*)&OrthoMatrix);
        }
                
        v2 PrimaryCursorScreenPosition = CursorTextToScreen(&Lines, PrimaryCursorPos);
        v2 SecondaryCursorScreenPosition = CursorTextToScreen(&Lines, SecondaryCursorPos);
        CursorDraw(Box, &Arena, PrimaryCursorScreenPosition, 0.0f, CursorVAO, CursorVBO, CursorShader);
        CursorDraw(Box, &Arena, SecondaryCursorScreenPosition, 1.0f, CursorVAO, CursorVBO, CursorShader);

        TextBoxRenderState RenderState = TextBoxBeginDraw(Box, &Arena, &Lines, TextVAO, TextVBO, TextShader);

        TextBoxPushText(RenderState, SB.Start, SB.Middle, v3(1,0,1));
        TextBoxPushText(RenderState, SB.Start + SB.Second, SB.TextSize - SB.Middle, TextColor);
        TextBoxEndDraw(RenderState);
        
        glFlush();
        SwapBuffers(Device_Context);
        FrameArenaReset(Arena);
    }

    FreeMemory(SB.Start);
    FrameArenaDelete(Arena);
    return 0;
}
