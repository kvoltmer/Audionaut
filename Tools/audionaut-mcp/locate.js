//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

// Finds the binary that runs Audionaut's CLI verbs and prepares the paths
// handed to it. Kept apart from index.js so the tests can exercise it without
// starting a server.
//
// Any Audionaut app runs the verbs itself (`Audionaut <verb> ... --json`), so
// an installed app is all an end user needs. The standalone audionaut-cli is
// the developer build and wins when present: it is not sandboxed.

import { execFile } from "node:child_process";
import { access, constants, existsSync, readdirSync, realpathSync } from "node:fs";
import { homedir } from "node:os";
import { delimiter, dirname, isAbsolute, join, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { promisify } from "node:util";

const accessAsync = promisify(access);
const execFileAsync = promisify(execFile);

const bundleId = "com.voltmer-systems.audionaut";

async function isExecutable(file) {
  try {
    await accessAsync(file, constants.X_OK);
    return true;
  } catch {
    return false;
  }
}

async function findOnPath(name, env, platform) {
  const extensions = platform === "win32" ? (env.PATHEXT || ".EXE;.CMD;.BAT").split(";") : [""];
  for (const dir of (env.PATH || "").split(delimiter).filter(Boolean))
    for (const extension of extensions) {
      const candidate = join(dir, name + extension);
      if (await isExecutable(candidate)) return candidate;
    }
  return null;
}

// The macOS app is sandboxed (Mac App Store and GitHub DMG alike): it can
// only reach ~/Music, so paths are checked before it is started.
export function isSandboxedApp(binary, platform = process.platform) {
  return platform === "darwin" && binary.includes(".app/Contents/MacOS/");
}

function macAppBinary(app) {
  return join(app, "Contents", "MacOS", "Audionaut");
}

async function spotlightApps() {
  try {
    const { stdout } = await execFileAsync("mdfind", [`kMDItemCFBundleIdentifier == '${bundleId}'`], {
      timeout: 2000,
    });
    return stdout.split("\n").filter((line) => line.endsWith(".app"));
  } catch {
    return [];
  }
}

function appImagesIn(dir) {
  try {
    return readdirSync(dir)
      .filter((name) => /^Audionaut.*\.AppImage$/i.test(name))
      .sort()
      .reverse() // newest version first
      .map((name) => join(dir, name));
  } catch {
    return [];
  }
}

// Returns { path, sandboxed, source, searched } - path is null when nothing
// was found, and error is set when AUDIONAUT_CLI points nowhere.
export async function resolveCli({
  env = process.env,
  platform = process.platform,
  home = homedir(),
  repoRoot = join(dirname(fileURLToPath(import.meta.url)), "..", ".."),
  spotlight = spotlightApps,
} = {}) {
  const searched = [];
  const found = (path, source) => ({ path, sandboxed: isSandboxedApp(path, platform), source, searched });

  if (env.AUDIONAUT_CLI) {
    if (await isExecutable(env.AUDIONAUT_CLI)) return found(env.AUDIONAUT_CLI, "AUDIONAUT_CLI");
    return {
      path: null,
      sandboxed: false,
      source: "AUDIONAUT_CLI",
      searched: [env.AUDIONAUT_CLI],
      error: `AUDIONAUT_CLI is set to "${env.AUDIONAUT_CLI}", but no executable is there.`,
    };
  }

  // Developer checkout: the CMake build of audionaut-cli. Single-config
  // generators emit AudionautCli_artefacts/ directly, multi-config ones add a
  // configuration subdirectory. Only looked for inside the repo, so a copy
  // installed by npx never probes ../../build.
  if (existsSync(join(repoRoot, "Audionaut", "Audionaut.jucer"))) {
    const artefacts = join(repoRoot, "build", "AudionautCli_artefacts");
    const binary = platform === "win32" ? "AudionautCli.exe" : "AudionautCli";
    for (const candidate of [join(artefacts, binary), join(artefacts, "Release", binary), join(artefacts, "Debug", binary)]) {
      searched.push(candidate);
      if (await isExecutable(candidate)) return found(candidate, "repo build");
    }
  }

  searched.push("audionaut-cli on PATH");
  const onPath = await findOnPath("audionaut-cli", env, platform);
  if (onPath) return found(onPath, "PATH");

  if (platform === "darwin") {
    const apps = [join("/Applications", "Audionaut.app"), join(home, "Applications", "Audionaut.app")];
    for (const app of apps) {
      searched.push(app);
      if (await isExecutable(macAppBinary(app))) return found(macAppBinary(app), "installed app");
    }
    searched.push(`Spotlight (${bundleId})`);
    for (const app of await spotlight())
      if (await isExecutable(macAppBinary(app))) return found(macAppBinary(app), "installed app (Spotlight)");
  } else if (platform === "win32") {
    // The NSIS installer's InstallDir is $ProgramFiles64\Audionaut.
    const roots = [env.ProgramW6432, env.ProgramFiles, env.LOCALAPPDATA && join(env.LOCALAPPDATA, "Programs")];
    for (const root of roots.filter(Boolean)) {
      const candidate = join(root, "Audionaut", "Audionaut.exe");
      searched.push(candidate);
      if (await isExecutable(candidate)) return found(candidate, "installed app");
    }
  } else {
    // The .deb installs /usr/bin/audionaut; AppImages live wherever the user
    // put them, so only the usual spots are tried.
    searched.push("audionaut on PATH");
    const deb = await findOnPath("audionaut", env, platform);
    if (deb) return found(deb, "installed app");
    for (const dir of [join(home, "Applications"), join(home, ".local", "bin")]) {
      searched.push(join(dir, "Audionaut*.AppImage"));
      for (const image of appImagesIn(dir))
        if (await isExecutable(image)) return found(image, "AppImage");
    }
  }

  return { path: null, sandboxed: false, source: null, searched };
}

export function notFoundMessage(cli) {
  return (
    (cli.error ? `${cli.error} ` : "Audionaut was not found. ") +
    "Install the app from https://audionaut.app/download (macOS: into Applications; Windows: the Setup " +
    "installer; Linux: the .deb, or set AUDIONAUT_CLI to your AppImage), or set AUDIONAUT_CLI to an " +
    `Audionaut or audionaut-cli binary. Searched: ${cli.searched.join(", ")}`
  );
}

// Makes a path argument absolute, expanding a leading ~ here: inside the
// macOS sandbox the app's own $HOME is its container, not the user's home.
export function userPath(path, { home = homedir(), cwd = process.cwd() } = {}) {
  if (path === "~") return home;
  if (path.startsWith("~/") || path.startsWith("~\\")) return join(home, path.slice(2));
  return isAbsolute(path) ? path : resolve(cwd, path);
}

function realpathOfNearestExisting(path) {
  let current = path;
  for (;;) {
    try {
      return realpathSync.native(current);
    } catch {
      const parent = dirname(current);
      if (parent === current) return current;
      current = parent;
    }
  }
}

// The first of the given absolute paths the sandboxed app cannot reach, or
// null. A path that does not exist yet (an export target, a new project)
// counts by its nearest existing parent; symlinks count by where they lead.
export function outsideMusicFolder(paths, { home = homedir() } = {}) {
  const music = realpathOfNearestExisting(join(home, "Music"));
  for (const path of paths) {
    const real = realpathOfNearestExisting(path);
    if (real !== music && !real.startsWith(music + sep)) return path;
  }
  return null;
}
