import sys.*;
import sys.io.*;
import haxe.*;
import haxe.io.*;
using StringTools;

enum Ci {
	TravisCI;
	AppVeyor;
}

class RunCi {
	static function successMsg(msg:String):Void {
		Sys.println('\x1b[32m' + msg + '\x1b[0m');
	}
	static function failMsg(msg:String):Void {
		Sys.println('\x1b[31m' + msg + '\x1b[0m');
	}
	static function infoMsg(msg:String):Void {
		Sys.println('\x1b[36m' + msg + '\x1b[0m');
	}

	/**
		Run a command using `Sys.command()`.
		If the command exits with non-zero code, exit the whole script with the same code.

		If `useRetry` is `true`, the command will be re-run if it exits with non-zero code (3 trials).
		It is useful for running network-dependent commands.
	*/
	static function runCommand(cmd:String, args:Array<String>, useRetry:Bool = false):Void {
		var trials = useRetry ? 3 : 1;
		var exitCode:Int = 1;

		while (trials-->0) {
			Sys.println('Command: $cmd $args');

			var t = Timer.stamp();
			exitCode = Sys.command(cmd, args);
			var dt = Math.round(Timer.stamp() - t);

			if (exitCode == 0)
				successMsg('Command exited with $exitCode in ${dt}s: $cmd $args');
			else
				failMsg('Command exited with $exitCode in ${dt}s: $cmd $args');

			if (exitCode == 0) {
				return;
			} else if (trials > 0) {
				Sys.println('Command will be re-run...');
			}
		}

		Sys.exit(exitCode);
	}

	static function commandResult(cmd:String, args:Array<String>):{
		stdout:String,
		stderr:String,
		exitCode:Int
	} {
		var p = new Process(cmd, args);
		var out = {
			stdout: p.stdout.readAll().toString(),
			stderr: p.stderr.readAll().toString(),
			exitCode: p.exitCode()
		}
		p.close();
		return out;
	}

	static var ci(default, never):Null<Ci> =
		if (Sys.getEnv("TRAVIS") == "true")
			TravisCI;
		else if (Sys.getEnv("APPVEYOR") == "True")
			AppVeyor;
		else
			null;
	static var gitInfo(get, null):{repo:String, branch:String, commit:String, timestamp:Float, date:String};
	static function get_gitInfo() return if (gitInfo != null) gitInfo else gitInfo = {
		repo: switch (ci) {
			case TravisCI:
				Sys.getEnv("TRAVIS_REPO_SLUG");
			case AppVeyor:
				Sys.getEnv("APPVEYOR_PROJECT_SLUG");
			case _:
				commandResult("git", ["config", "--get", "remote.origin.url"]).stdout.trim();
		},
		branch: switch (ci) {
			case TravisCI:
				Sys.getEnv("TRAVIS_BRANCH");
			case AppVeyor:
				Sys.getEnv("APPVEYOR_REPO_BRANCH");
			case _:
				commandResult("git", ["rev-parse", "--abbrev-ref", "HEAD"]).stdout.trim();
		},
		commit: commandResult("git", ["rev-parse", "HEAD"]).stdout.trim(),
		timestamp: Std.parseFloat(commandResult("git", ["show", "-s", "--format=%ct", "HEAD"]).stdout),
		date: {
			var gitTime = commandResult("git", ["show", "-s", "--format=%ct", "HEAD"]).stdout;
			var tzd = {
				var z = Date.fromTime(0);
				z.getHours() * 60 * 60 * 1000 + z.getMinutes() * 60 * 1000;
			}
			// make time in the UTC time zone
			var time = Date.fromTime(Std.parseFloat(gitTime) * 1000 - tzd);
			DateTools.format(time, "%FT%TZ");
		}
	}

	static function bintray():Void {
		if (
			Sys.getEnv("BINTRAY_USERNAME") != null &&
			Sys.getEnv("BINTRAY_API_KEY") != null
		) {

			// generate bintray config
			var tpl = new Template(File.getContent("bintray.tpl.json"));
			var compatDate = ~/[^0-9]/g.replace(gitInfo.date, "");
			var nekoVer = commandResult("neko", ["-version"]).stdout.trim();
			var json = tpl.execute({
				packageSubject: {
					var sub = Sys.getEnv("BINTRAY_SUBJECT");
					sub != null ? sub : Sys.getEnv("BINTRAY_USERNAME");
				},
				os: Sys.systemName().toLowerCase(),
				versionName: '${nekoVer}+${compatDate}.${gitInfo.commit.substr(0,7)}',
				versionDesc: "Automated CI build.",
				gitRepo: gitInfo.repo,
				gitBranch: gitInfo.branch,
				gitCommit: gitInfo.commit,
				gitDate: gitInfo.date,
			});
			var path = FileSystem.absolutePath("bintray.json");
			File.saveContent(path, json);
			infoMsg("saved " + path + " with content:");
			Sys.println(json);
		}
	}

	static function main():Void {
		bintray();
	}
}