/* Inline shaders in effects (patches/sg/6543). An effect's pass that names
 * its shaders with SetVertexShader(CompileShader(vs_4_0, VS())) did not
 * compile with fx_5_0 ("Unsupported assignment type for state VertexShader"),
 * and fx_4_0 wrote the pass without its shaders. Kodi's yuv2rgb effect is
 * written that way: its video was black.
 * The probe compiles such an effect with fx_4_0 and fx_5_0 (D3DCompile), finds
 * the shader blobs the effect binary carries and reflects them (a vs_4_0 that
 * takes POSITION, a ps_4_0), and creates the fx_4_0 effect with D3D10: the
 * pass's shaders are there. */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <d3d10.h>
#include <stdio.h>
#include <string.h>

static const char effect[] =
    "float4 colour;\n"
    "struct VS_IN { float4 pos : POSITION; float2 uv : TEXCOORD0; };\n"
    "struct VS_OUT { float2 uv : TEXCOORD0; float4 pos : SV_POSITION; };\n"
    "VS_OUT VS(VS_IN i) { VS_OUT o; o.pos = i.pos; o.uv = i.uv; return o; }\n"
    "float4 PS(VS_OUT i) : SV_TARGET { return colour * float4(i.uv, 0, 1); }\n"
    "#define VS_SHADER CompileShader(vs_4_0, VS())\n"
    "TECH T0\n"
    "{\n"
    "    pass P0\n"
    "    {\n"
    "        SetVertexShader(VS_SHADER);\n"
    "        SetPixelShader(CompileShader(ps_4_0, PS()));\n"
    "    }\n"
    "};\n";

static int check_blobs( const char *profile, const BYTE *data, SIZE_T size, int *vs_ok, int *ps_ok )
{
    SIZE_T i;
    int blobs = 0;

    *vs_ok = *ps_ok = 0;
    for (i = 0; i + 32 <= size; i++)
    {
        ID3D11ShaderReflection *refl;
        D3D11_SHADER_DESC desc;
        DWORD len;

        if (memcmp( data + i, "DXBC", 4 )) continue;
        memcpy( &len, data + i + 24, 4 );
        if (len < 32 || i + len > size) continue;
        /* (an fx_4 effect is itself in a DXBC container: look inside it) */
        if (FAILED(D3DReflect( data + i, len, &IID_ID3D11ShaderReflection, (void **)&refl ))) continue;
        blobs++;
        if (SUCCEEDED(refl->lpVtbl->GetDesc( refl, &desc )))
        {
            UINT type = (desc.Version >> 16) & 0xffff, major = (desc.Version >> 4) & 0xf;  /* D3D11_SHVER_GET_TYPE/MAJOR */
            if (type == 1 /* D3D11_SHVER_VERTEX_SHADER */ && major == 4)
            {
                D3D11_SIGNATURE_PARAMETER_DESC p;
                if (desc.InputParameters >= 1 &&
                    SUCCEEDED(refl->lpVtbl->GetInputParameterDesc( refl, 0, &p )) &&
                    !strcmp( p.SemanticName, "POSITION" ))
                    *vs_ok = 1;
            }
            if (type == 0 /* D3D11_SHVER_PIXEL_SHADER */ && major == 4) *ps_ok = 1;
        }
        refl->lpVtbl->Release( refl );
    }
    printf( "%s: blobs=%d\n", profile, blobs );
    return blobs;
}

static ID3DBlob *compile( const char *profile, const char *tech )
{
    D3D_SHADER_MACRO defs[] = { { "TECH", tech }, { NULL, NULL } };
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr = D3DCompile( effect, sizeof(effect) - 1, "inline.fx", defs, NULL, NULL, profile, 0, 0, &code, &err );
    printf( "%s: hr=%#lx\n", profile, hr );
    if (err) { printf( "%s: %s\n", profile, (char *)ID3D10Blob_GetBufferPointer( err ) ); ID3D10Blob_Release( err ); }
    return SUCCEEDED(hr) ? code : NULL;
}

int main( void )
{
    ID3DBlob *fx4, *fx5;
    int vs, ps;
    ID3D10Device *device = NULL;

    if ((fx5 = compile( "fx_5_0", "technique11" )))
    {
        check_blobs( "fx_5_0", ID3D10Blob_GetBufferPointer( fx5 ), ID3D10Blob_GetBufferSize( fx5 ), &vs, &ps );
        printf( "fx5_compiles=1 fx5_vs=%d fx5_ps=%d\n", vs, ps );
    }
    else printf( "fx5_compiles=0\n" );

    if ((fx4 = compile( "fx_4_0", "technique10" )))
    {
        check_blobs( "fx_4_0", ID3D10Blob_GetBufferPointer( fx4 ), ID3D10Blob_GetBufferSize( fx4 ), &vs, &ps );
        printf( "fx4_compiles=1 fx4_vs=%d fx4_ps=%d\n", vs, ps );
    }
    else printf( "fx4_compiles=0\n" );

    if (fx4 && SUCCEEDED(D3D10CreateDevice( NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &device )))
    {
        ID3D10Effect *e = NULL;
        HRESULT hr = D3D10CreateEffectFromMemory( ID3D10Blob_GetBufferPointer( fx4 ), ID3D10Blob_GetBufferSize( fx4 ),
                                                  0, device, NULL, &e );
        int pass_vs = 0, pass_ps = 0;
        if (SUCCEEDED(hr))
        {
            ID3D10EffectPass *pass = ID3D10EffectTechnique_GetPassByIndex(
                    ID3D10Effect_GetTechniqueByIndex( e, 0 ), 0 );
            D3D10_PASS_SHADER_DESC sd;
            D3D10_EFFECT_SHADER_DESC desc;
            if (SUCCEEDED(ID3D10EffectPass_GetVertexShaderDesc( pass, &sd )) && sd.pShaderVariable &&
                SUCCEEDED(ID3D10EffectShaderVariable_GetShaderDesc( sd.pShaderVariable, sd.ShaderIndex, &desc )))
                pass_vs = desc.pBytecode && desc.BytecodeLength > 0;
            if (SUCCEEDED(ID3D10EffectPass_GetPixelShaderDesc( pass, &sd )) && sd.pShaderVariable &&
                SUCCEEDED(ID3D10EffectShaderVariable_GetShaderDesc( sd.pShaderVariable, sd.ShaderIndex, &desc )))
                pass_ps = desc.pBytecode && desc.BytecodeLength > 0;
            ID3D10Effect_Release( e );
        }
        printf( "d3d10_effect=%d pass_vs=%d pass_ps=%d (hr %#lx)\n", SUCCEEDED(hr), pass_vs, pass_ps, hr );
        ID3D10Device_Release( device );
    }
    else printf( "d3d10 device: none\n" );
    printf( "done=1\n" );
    return 0;
}
