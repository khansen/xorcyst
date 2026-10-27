#!/usr/bin/env python3
"""Build a pinned reference from local Git history; never downloads or installs."""
import argparse
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import signal
import stat
import subprocess
import tarfile
import tempfile

from common import stop_process

ROOT = Path(__file__).resolve().parents[2]
PIN = json.loads(Path(__file__).with_name('baseline.json').read_text())


def default_cache():
    if platform.system() == 'Darwin':
        base = Path.home() / 'Library' / 'Caches'
    else:
        configured = Path(os.environ.get('XDG_CACHE_HOME', ''))
        base = configured if configured.is_absolute() else Path.home() / '.cache'
    return base / 'xorcyst' / 'reference'


def require_owned(path, directory=False):
    info = path.lstat()
    expected_type = stat.S_ISDIR if directory else stat.S_ISREG
    if (not expected_type(info.st_mode) or info.st_uid != os.getuid()
            or info.st_mode & 0o022):
        raise ValueError(f'cache entry must be owned by the current user and not writable by others: {path}')


def private_cache(path):
    path = Path(path).expanduser().absolute()
    if path.is_symlink():
        raise ValueError(f'cache directory cannot be a symlink: {path}')
    missing = []
    parent = path.parent
    while not parent.exists():
        missing.append(parent)
        parent = parent.parent
    # Validate existing ancestors before creating anything below them. In
    # particular, do not "repair" directories supplied by another user.
    parent = parent.resolve()
    for parent in (parent, *parent.parents):
        info = parent.stat()
        if not stat.S_ISDIR(info.st_mode):
            raise ValueError(f'cache parent is not a directory: {parent}')
        if info.st_uid not in (0, os.getuid()):
            raise ValueError(f'cache parent is owned by another user: {parent}')
        # Sticky shared temporary directories protect owned children; ordinary
        # group/world-writable parents would allow replacement of the cache.
        if info.st_mode & 0o022 and not info.st_mode & stat.S_ISVTX:
            raise ValueError(f'cache has an unsafe writable parent: {parent}')
    # mkdir(parents=True) applies mode only to the leaf. Create each missing
    # directory privately, and only chmod directories this invocation created.
    for directory in [*reversed(missing), path]:
        try:
            directory.mkdir(mode=0o700)
        except FileExistsError:
            require_owned(directory, directory=True)
        else:
            directory.chmod(0o700)
    return path.resolve()


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


@contextmanager
def reference_lock(path):
    """Keep one persistent lock inode per key, including across failed builds."""
    try:
        fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    except FileExistsError:
        require_owned(path)
        fd = os.open(path, os.O_RDWR | os.O_NOFOLLOW)
    else:
        os.fchmod(fd, 0o600)
    with os.fdopen(fd, 'r+b') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield


def run_build_command(command, timeout=180, **kwargs):
    """Stop descendants and reap the build driver before scratch cleanup."""
    proc = subprocess.Popen(command, start_new_session=True, **kwargs)
    try:
        returncode = proc.wait(timeout=timeout)
        if returncode:
            raise subprocess.CalledProcessError(returncode, command)
    except BaseException:
        stop_process(proc)
        proc.wait()
        raise


def build_reference(cache, repo=ROOT):
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    if not compiler or not shutil.which(compiler[0]):
        raise ValueError('CC must name an available C compiler')
    compiler[0] = str(Path(shutil.which(compiler[0])).resolve())
    flags = os.environ.get('CFLAGS', PIN['cflags'])
    identity = {
        'revision': PIN['revision'], 'compiler': compiler, 'cflags': flags,
        'compiler_version': subprocess.check_output([*compiler, '--version'], text=True),
        'host': [platform.system(), platform.release(), platform.machine()],
        'recipe_sha256': digest(__file__),
        'build_environment': {k: os.environ.get(k, '') for k in
                              ('CPPFLAGS', 'LDFLAGS', 'SDKROOT', 'MACOSX_DEPLOYMENT_TARGET')},
    }
    key = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
    cache = private_cache(cache)
    # Hold the lock through lookup, compilation, logging and publication. A
    # waiting caller validates and reuses the winner instead of building twice.
    with reference_lock(cache / (key + '.lock')):
        return build_locked_reference(cache, key, identity, repo)


def build_locked_reference(cache, key, identity, repo):
    target = cache / key
    receipt = target / 'build.json'
    if target.exists() or target.is_symlink():
        require_owned(target, directory=True)
    if receipt.exists():
        for path in (receipt, target / 'xasm', target / 'xlnk'):
            require_owned(path)
        saved = json.loads(receipt.read_text())
        if (saved['identity'] != identity or any(
                not (target / name).is_file() or digest(target / name) != saved['sha256'][name]
                for name in ('xasm', 'xlnk'))):
            raise ValueError(f'cached reference failed integrity check: {target}')
        return target
    # A cache miss requires the pinned source revision; never substitute HEAD.
    subprocess.run(['git', '-C', str(repo), 'cat-file', '-e', PIN['revision'] + '^{commit}'],
                   check=True, capture_output=True)
    with tempfile.TemporaryDirectory(prefix='reference-', dir=cache) as tmp:
        work = Path(tmp)
        archive = work / 'source.tar'
        with archive.open('wb') as stream:
            subprocess.run(['git', '-C', str(repo), 'archive', PIN['revision']],
                           stdout=stream, check=True)
        source = work / 'source'
        source.mkdir(mode=0o700)
        with tarfile.open(archive) as tar:
            for member in tar:
                # The reference needs only regular files/directories; do not
                # permit archive links or paths escaping the scratch directory.
                path = Path(member.name)
                if path.is_absolute() or '..' in path.parts or not (member.isfile() or member.isdir()):
                    raise ValueError(f'unsafe reference archive entry: {member.name}')
                tar.extract(member, source)
        env = {k: v for k, v in os.environ.items() if k in
               ('PATH', 'SDKROOT', 'MACOSX_DEPLOYMENT_TARGET', 'CPPFLAGS', 'LDFLAGS')}
        env.update(CC=shlex.join(identity['compiler']), CFLAGS=identity['cflags'], LC_ALL='C', TZ='UTC',
                   HOME=str(work), TMPDIR=str(work))
        commands = [
            ['./configure', '--disable-maintainer-mode', 'YACC=:', 'LEX=:', 'HELP2MAN=:'],
            ['make', '-j2'],
        ]
        # Preserve build logs on failures as well as successful builds.
        with (cache / (key + '.log')).open('wb') as log:
            os.fchmod(log.fileno(), 0o644)
            for command in commands:
                log.write((shlex.join(command) + '\n').encode()); log.flush()
                run_build_command(command, cwd=source, env=env, stdout=log,
                                  stderr=subprocess.STDOUT)
        packaged = work / 'tools'
        packaged.mkdir(mode=0o700)
        packaged.chmod(0o700)
        for name in ('xasm', 'xlnk'):
            shutil.copy2(source / name, packaged / name)
            (packaged / name).chmod(0o755)
        data = {'identity': identity, 'commands': commands,
                'sha256': {name: digest(packaged / name) for name in ('xasm', 'xlnk')}}
        (packaged / 'build.json').write_text(json.dumps(data, indent=2) + '\n')
        (packaged / 'build.json').chmod(0o644)
        packaged.rename(target)
    return target


def interrupted(signum, frame):
    raise KeyboardInterrupt(f'received signal {signum}')


if __name__ == '__main__':
    signal.signal(signal.SIGTERM, interrupted)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    args = parser.parse_args()
    print(build_reference(args.cache))
