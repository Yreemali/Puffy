import { spawnSync } from 'node:child_process'
import { cpSync, existsSync, mkdirSync, readdirSync, rmSync, writeFileSync } from 'node:fs'
import { basename, join, resolve } from 'node:path'
import process from 'node:process'

const webRoot = resolve(import.meta.dirname, '..')
const projectRoot = resolve(webRoot, '..', '..')
const tauriRoot = join(webRoot, 'src-tauri')
const nativeStage = join(tauriRoot, 'native')
const buildDir = resolve(projectRoot, process.env.PUFFY_NATIVE_BUILD_DIR || 'build-tauri-native')
const platform = process.platform

function run(command, args, options = {}) {
  const result = spawnSync(command, args, { cwd: options.cwd ?? projectRoot, env: process.env, stdio: 'inherit', shell: false })
  if (result.error) throw result.error
  if (result.status !== 0) process.exit(result.status ?? 1)
}

function filesBelow(directory) {
  if (!existsSync(directory)) return []
  return readdirSync(directory, { withFileTypes: true }).flatMap(entry => {
    const path = join(directory, entry.name)
    return entry.isDirectory() ? filesBelow(path) : [path]
  })
}

function newestMatch(pattern) {
  const matches = filesBelow(buildDir).filter(path => pattern.test(basename(path)) && !path.includes(`${join('CMakeFiles', '')}`))
  if (!matches.length) throw new Error(`Native build did not produce ${pattern}`)
  return matches[0]
}

function configureNative() {
  const args = ['-S', projectRoot, '-B', buildDir, '-DPUFFY_BUILD_TESTS=OFF', '-DCMAKE_BUILD_TYPE=Release']
  const vcpkgRoot = process.env.VCPKG_ROOT || process.env.VCPKG_INSTALLATION_ROOT
  if (platform === 'win32' && vcpkgRoot) {
    args.push(`-DCMAKE_TOOLCHAIN_FILE=${join(vcpkgRoot, 'scripts', 'buildsystems', 'vcpkg.cmake')}`)
    args.push(`-DVCPKG_TARGET_TRIPLET=${process.env.VCPKG_TARGET_TRIPLET || 'x64-windows-static'}`)
  }
  run('cmake', args)
  run('cmake', ['--build', buildDir, '--config', 'Release', '--target', 'puffy_native', '--parallel'])
}

function commandOutput(command, args) {
  const result = spawnSync(command, args, { encoding: 'utf8', env: process.env })
  if (result.status !== 0) throw new Error(result.stderr || `${command} failed`)
  return result.stdout
}

function macDependencies(rootLibrary) {
  const staged = new Map()
  const queue = [rootLibrary]
  while (queue.length) {
    const source = queue.shift()
    const name = basename(source)
    if (staged.has(name)) continue
    const destination = join(nativeStage, name)
    cpSync(source, destination)
    staged.set(name, { source, destination })
    const lines = commandOutput('otool', ['-L', source]).split('\n').slice(1)
    for (const line of lines) {
      const dependency = line.trim().split(' ')[0]
      if (!dependency || dependency.startsWith('/usr/lib/') || dependency.startsWith('/System/') || dependency.startsWith('@')) continue
      if (existsSync(dependency)) queue.push(dependency)
    }
  }
  for (const { source, destination } of staged.values()) {
    run('install_name_tool', ['-id', `@rpath/${basename(destination)}`, destination])
    const lines = commandOutput('otool', ['-L', source]).split('\n').slice(1)
    for (const line of lines) {
      const dependency = line.trim().split(' ')[0]
      if (dependency && staged.has(basename(dependency))) {
        run('install_name_tool', ['-change', dependency, `@rpath/${basename(dependency)}`, destination])
      }
    }
  }
  return [...staged.values()].map(item => item.destination)
}

function stageNative() {
  rmSync(nativeStage, { recursive: true, force: true })
  mkdirSync(nativeStage, { recursive: true })
  if (platform === 'win32') {
    cpSync(newestMatch(/^puffy_native\.dll$/i), join(nativeStage, 'puffy_native.dll'))
    cpSync(newestMatch(/^puffy_native\.lib$/i), join(nativeStage, 'puffy_native.lib'))
    const certificateThumbprint = process.env.PUFFY_WINDOWS_CERTIFICATE_THUMBPRINT
    const timestampUrl = process.env.PUFFY_WINDOWS_TIMESTAMP_URL
    if ((certificateThumbprint && !timestampUrl) || (!certificateThumbprint && timestampUrl)) {
      throw new Error('Set both PUFFY_WINDOWS_CERTIFICATE_THUMBPRINT and PUFFY_WINDOWS_TIMESTAMP_URL for signed builds')
    }
    const windows = { bundleVCRuntime: true, webviewInstallMode: { type: 'downloadBootstrapper' } }
    if (certificateThumbprint) {
      windows.certificateThumbprint = certificateThumbprint
      windows.digestAlgorithm = 'sha256'
      windows.timestampUrl = timestampUrl
    }
    return { resources: { 'native/puffy_native.dll': 'puffy_native.dll' }, windows }
  }
  if (platform === 'darwin') {
    const libraries = macDependencies(newestMatch(/^libpuffy_native\.dylib$/))
    return { macOS: { minimumSystemVersion: '12.0', frameworks: libraries.map(path => `./native/${basename(path)}`) } }
  }
  cpSync(newestMatch(/^libpuffy_native\.so(?:\..*)?$/), join(nativeStage, 'libpuffy_native.so'))
  return { resources: { 'native/libpuffy_native.so': 'libpuffy_native.so' } }
}

function writePlatformConfig(bundle) {
  const path = join(tauriRoot, 'generated.platform.conf.json')
  writeFileSync(path, `${JSON.stringify({ bundle }, null, 2)}\n`)
  return path
}

function prepare() {
  configureNative()
  return writePlatformConfig(stageNative())
}

const mode = process.argv[2] ?? 'prepare'
const config = prepare()
if (mode === 'prepare') process.exit(0)
if (mode !== 'dev' && mode !== 'build') throw new Error(`Unknown Tauri mode: ${mode}`)
const tauri = join(webRoot, 'node_modules', '.bin', platform === 'win32' ? 'tauri.cmd' : 'tauri')
run(tauri, [mode, '--config', config, ...process.argv.slice(3)], { cwd: webRoot })
