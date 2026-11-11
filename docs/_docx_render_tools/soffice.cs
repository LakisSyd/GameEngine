using System;
using System.IO;
using System.Runtime.InteropServices;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        string outputDirectory = null;
        string format = null;
        string inputPath = null;

        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--outdir" && i + 1 < args.Length)
                outputDirectory = args[++i];
            else if (args[i] == "--convert-to" && i + 1 < args.Length)
                format = args[++i];
            else if (!args[i].StartsWith("-", StringComparison.Ordinal))
                inputPath = args[i];
        }

        if (outputDirectory == null || inputPath == null || format == null ||
            !format.StartsWith("pdf", StringComparison.OrdinalIgnoreCase))
        {
            Console.Error.WriteLine("Word-backed soffice shim needs PDF format, --outdir and an input file.");
            return 2;
        }

        inputPath = Path.GetFullPath(inputPath);
        outputDirectory = Path.GetFullPath(outputDirectory);
        Directory.CreateDirectory(outputDirectory);
        string outputPath = Path.Combine(
            outputDirectory,
            Path.GetFileNameWithoutExtension(inputPath) + ".pdf");

        dynamic word = null;
        dynamic document = null;
        try
        {
            Type wordType = Type.GetTypeFromProgID("Word.Application");
            if (wordType == null)
                throw new InvalidOperationException("Microsoft Word COM server is unavailable.");

            word = Activator.CreateInstance(wordType);
            word.Visible = false;
            word.DisplayAlerts = 0;
            word.Options.CheckSpellingAsYouType = false;
            word.Options.CheckGrammarAsYouType = false;
            word.Options.SaveNormalPrompt = false;
            document = word.Documents.Open(inputPath, false, true, false);
            document.ExportAsFixedFormat(outputPath, 17);
            document.Close(false);
            document = null;
            word.Quit();
            word = null;
            Console.WriteLine("convert " + inputPath + " -> " + outputPath + " using Microsoft Word");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.ToString());
            return 1;
        }
        finally
        {
            if (document != null)
            {
                try { document.Close(false); } catch { }
            }
            if (word != null)
            {
                try { word.Quit(); } catch { }
            }
        }
    }
}
