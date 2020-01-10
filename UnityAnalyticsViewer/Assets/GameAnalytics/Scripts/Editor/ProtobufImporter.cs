using UnityEngine;
using UnityEditor;
using System;
using System.IO;

internal sealed class ProtoProcessor : AssetPostprocessor
{
    static void OnPostprocessAllAssets(string[] importedAssets, string[] deletedAssets, string[] movedAssets, string[] movedFromAssetPaths)
    {
        foreach (string assetPath in importedAssets)
        {
            string asset = assetPath.ToLower();
            if (asset.EndsWith(".proto"))
            {
                CompileProto(asset);
            }
        }
    }

    static void CompileProto(string asset)
    {
        try
        {
            // D:\git\protobuf/compiler/protoc.exe -I=%(RootDir)%(Directory) -I=D:\git\protobuf/src --cpp_out=%(RootDir)%(Directory) %(FullPath)

            string pbSrc = "E:\\p4\\wildcard\\development\\Plugins\\GameAnalytics\\Source\\protobuf\\include";
            string assetPath = Path.GetDirectoryName(Application.dataPath);
            string assetFile = Path.Combine(assetPath, asset.Replace("/", "\\"));
            string dir = Path.Combine(assetPath, Path.GetDirectoryName(asset));
            string exe = "protoc.exe";// Path.Combine(dir, "protoc.exe");
            string args = string.Format("-I=\"{0}\" -I=\"{1}\" --csharp_out=\"{0}\" {2}", dir, pbSrc, assetFile);
            
            Debug.LogFormat("Compiling Protobuf {0}\n{1}\n{2}", exe, asset, args);

            System.Diagnostics.Process proc = new System.Diagnostics.Process();
            //proc.StartInfo.WorkingDirectory = dir;
            proc.EnableRaisingEvents = true;
            proc.StartInfo.CreateNoWindow = true;
            proc.StartInfo.FileName = exe;
            proc.StartInfo.Arguments = args;
            proc.StartInfo.UseShellExecute = false;
            proc.StartInfo.RedirectStandardOutput = true;
            proc.StartInfo.RedirectStandardError = true;
            proc.Start();

            proc.BeginOutputReadLine();
            proc.BeginErrorReadLine();
            proc.OutputDataReceived += (sender, arg) => {
                if(arg.Data != null)
                    Debug.Log(arg.Data);
            };
            proc.ErrorDataReceived += (sender, arg) => {
                if (arg.Data != null)
                    Debug.LogError(arg.Data);
            };

            proc.WaitForExit();

            proc.BeginOutputReadLine();
        }
        catch (Exception ex)
        {
            Debug.LogErrorFormat("Error Compiling Proto file {0}\n{1}", asset, ex.Message);
        }
    }
}