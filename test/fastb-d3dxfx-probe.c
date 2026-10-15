/* d3dx9_36 effect compiler (patch 3591): parameters, techniques, passes and functions of an effect source,
 * values, literals, CompileEffect and CompileShader. */
#include <windows.h>
#define COBJMACROS
#include <d3dx9.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; printf("FAIL  %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } else printf("ok    %d\n", __LINE__); } while (0)

static const char effect_source[] =
    "float4 g_color : COLOR = float4(1, 0, 0, 1);\n"
    "float g_scale = 2.0;\n"
    "float4x4 g_world;\n"
    "texture g_tex;\n"
    "struct VS_OUT { float4 pos : POSITION; float4 col : COLOR0; };\n"
    "VS_OUT VSMain(float4 pos : POSITION)\n"
    "{\n"
    "    VS_OUT o;\n"
    "    o.pos = mul(pos, g_world);\n"
    "    o.col = g_color * g_scale;\n"
    "    return o;\n"
    "}\n"
    "float4 PSMain(VS_OUT i) : COLOR\n"
    "{\n"
    "    return i.col;\n"
    "}\n"
    "technique T0\n"
    "{\n"
    "    pass P0\n"
    "    {\n"
    "    }\n"
    "}\n";

int main(void)
{
    ID3DXEffectCompiler *compiler = NULL;
    ID3DXBuffer *errors = NULL, *buffer = NULL, *shader = NULL;
    ID3DXConstantTable *table = NULL;
    D3DXEFFECT_DESC desc;
    D3DXPARAMETER_DESC pdesc;
    D3DXTECHNIQUE_DESC tdesc;
    D3DXPASS_DESC passdesc;
    D3DXFUNCTION_DESC fdesc;
    D3DXHANDLE h, tech, pass, vs, ps;
    float f = 0.0f;
    BOOL literal;
    HRESULT hr;

    hr = D3DXCreateEffectCompiler(effect_source, sizeof(effect_source) - 1, NULL, NULL, 0, &compiler, &errors);
    CHECK(hr == D3D_OK && compiler, "D3DXCreateEffectCompiler %#lx", hr);
    if (FAILED(hr))
    {
        if (errors) printf("%s\n", (char *)errors->lpVtbl->GetBufferPointer(errors));
        printf("RESULT: FAIL\n");
        return 1;
    }
    hr = compiler->lpVtbl->GetDesc(compiler, &desc);
    CHECK(hr == D3D_OK && desc.Parameters == 4 && desc.Techniques == 1 && desc.Functions == 2,
            "GetDesc %#lx: %u parameters %u techniques %u functions", hr, desc.Parameters, desc.Techniques, desc.Functions);

    h = compiler->lpVtbl->GetParameterByName(compiler, NULL, "g_scale");
    CHECK(h != NULL, "GetParameterByName g_scale");
    hr = compiler->lpVtbl->GetParameterDesc(compiler, h, &pdesc);
    CHECK(hr == D3D_OK && !strcmp(pdesc.Name, "g_scale") && pdesc.Type == D3DXPT_FLOAT && pdesc.Class == D3DXPC_SCALAR,
            "GetParameterDesc %#lx", hr);
    hr = compiler->lpVtbl->GetFloat(compiler, h, &f);
    CHECK(hr == D3D_OK && f == 2.0f, "default value %g", f);
    hr = compiler->lpVtbl->SetFloat(compiler, h, 5.0f);
    CHECK(hr == D3D_OK, "SetFloat %#lx", hr);
    f = 0.0f;
    compiler->lpVtbl->GetFloat(compiler, "g_scale", &f);
    CHECK(f == 5.0f, "value after SetFloat %g", f);
    h = compiler->lpVtbl->GetParameterBySemantic(compiler, NULL, "COLOR");
    CHECK(h != NULL, "GetParameterBySemantic");
    h = compiler->lpVtbl->GetParameter(compiler, NULL, 2);
    compiler->lpVtbl->GetParameterDesc(compiler, h, &pdesc);
    CHECK(pdesc.Class == D3DXPC_MATRIX_ROWS, "third parameter is the matrix");

    hr = compiler->lpVtbl->SetLiteral(compiler, "g_scale", TRUE);
    CHECK(hr == D3D_OK, "SetLiteral %#lx", hr);
    literal = FALSE;
    hr = compiler->lpVtbl->GetLiteral(compiler, "g_scale", &literal);
    CHECK(hr == D3D_OK && literal, "GetLiteral");

    tech = compiler->lpVtbl->GetTechniqueByName(compiler, "T0");
    CHECK(tech != NULL, "GetTechniqueByName");
    hr = compiler->lpVtbl->GetTechniqueDesc(compiler, tech, &tdesc);
    CHECK(hr == D3D_OK && !strcmp(tdesc.Name, "T0") && tdesc.Passes == 1, "GetTechniqueDesc %#lx", hr);
    pass = compiler->lpVtbl->GetPass(compiler, tech, 0);
    CHECK(pass != NULL, "GetPass");
    hr = compiler->lpVtbl->GetPassDesc(compiler, pass, &passdesc);
    CHECK(hr == D3D_OK && !strcmp(passdesc.Name, "P0"), "GetPassDesc %#lx", hr);

    vs = compiler->lpVtbl->GetFunctionByName(compiler, "VSMain");
    ps = compiler->lpVtbl->GetFunction(compiler, 1);
    CHECK(vs != NULL && ps != NULL && vs != ps, "functions");
    hr = compiler->lpVtbl->GetFunctionDesc(compiler, vs, &fdesc);
    CHECK(hr == D3D_OK && !strcmp(fdesc.Name, "VSMain"), "GetFunctionDesc %#lx", hr);
    hr = compiler->lpVtbl->GetFunctionDesc(compiler, ps, &fdesc);
    CHECK(hr == D3D_OK && !strcmp(fdesc.Name, "PSMain"), "GetFunctionDesc second");
    CHECK(compiler->lpVtbl->GetFunction(compiler, 7) == NULL, "GetFunction out of range");
    CHECK(compiler->lpVtbl->GetFunctionByName(compiler, "Missing") == NULL, "GetFunctionByName missing");
    hr = compiler->lpVtbl->GetFunctionDesc(compiler, "Missing", &fdesc);
    CHECK(hr == D3DERR_INVALIDCALL, "GetFunctionDesc missing %#lx", hr);

    hr = compiler->lpVtbl->CompileEffect(compiler, 0, &buffer, &errors);
    CHECK(hr == D3D_OK && buffer && buffer->lpVtbl->GetBufferSize(buffer) > 16
            && *(DWORD *)buffer->lpVtbl->GetBufferPointer(buffer) == 0xfeff0901, "CompileEffect %#lx", hr);
    if (buffer) buffer->lpVtbl->Release(buffer);

    hr = compiler->lpVtbl->CompileShader(compiler, vs, "vs_2_0", 0, &shader, &errors, &table);
    CHECK(hr == D3D_OK && shader && *(DWORD *)shader->lpVtbl->GetBufferPointer(shader) == 0xfffe0200,
            "CompileShader %#lx", hr);
    if (errors && hr != D3D_OK) printf("%s\n", (char *)errors->lpVtbl->GetBufferPointer(errors));
    if (table)
    {
        D3DXCONSTANTTABLE_DESC cdesc;

        hr = table->lpVtbl->GetDesc(table, &cdesc);
        CHECK(hr == D3D_OK && cdesc.Constants >= 2, "constant table: %u constants", cdesc.Constants);
        table->lpVtbl->Release(table);
    }
    else
        CHECK(0, "no constant table");
    if (shader) shader->lpVtbl->Release(shader);
    hr = compiler->lpVtbl->CompileShader(compiler, "PSMain", "ps_2_0", 0, &shader, NULL, NULL);
    CHECK(hr == D3D_OK && shader && *(DWORD *)shader->lpVtbl->GetBufferPointer(shader) == 0xffff0200, "CompileShader by name %#lx", hr);
    if (shader) shader->lpVtbl->Release(shader);
    hr = compiler->lpVtbl->CompileShader(compiler, "Missing", "ps_2_0", 0, &shader, NULL, NULL);
    CHECK(hr == D3DERR_INVALIDCALL, "CompileShader missing %#lx", hr);

    compiler->lpVtbl->Release(compiler);

    /* an effect that does not compile */
    compiler = NULL;
    errors = NULL;
    hr = D3DXCreateEffectCompiler("float4 broken(", 14, NULL, NULL, 0, &compiler, &errors);
    CHECK(FAILED(hr) && !compiler, "broken source %#lx", hr);
    if (errors) errors->lpVtbl->Release(errors);

    printf("%d failures\nRESULT: %s\n", failures, failures ? "FAIL" : "PASS");
    return failures != 0;
}
