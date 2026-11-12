import { lstat, mkdir } from 'node:fs/promises';
import path from 'node:path';

/** @param {unknown} error */
export function absent(error) {
  return error instanceof Error && 'code' in error && error.code === 'ENOENT';
}

/** Resolve a project-relative path, rejecting escapes, symlinks and
 * non-directory parents. Missing leaves are allowed; missing parents are
 * created only when requested.
 * @param {string} root @param {string} relative @param {boolean} [createParents] */
export async function projectPath(root, relative, createParents = false) {
  if (
    !relative ||
    /[\x00-\x1f\\;"$]/.test(relative) ||
    path.isAbsolute(relative) ||
    path.win32.isAbsolute(relative) ||
    relative.split('/').some((part) => !part || part === '.' || part === '..')
  )
    throw new Error(
      'Use a project-relative path without escapes, control characters or CMake metacharacters.'
    );
  let current = root;
  const parts = relative.split('/');
  for (const [index, part] of parts.entries()) {
    current = path.join(current, part);
    const parent = index < parts.length - 1;
    let stat;
    try {
      stat = await lstat(current);
    } catch (error) {
      if (!absent(error)) throw error;
      if (parent && createParents) {
        try {
          await mkdir(current);
        } catch (created) {
          if (!(created instanceof Error && 'code' in created && created.code === 'EEXIST'))
            throw created;
        }
        stat = await lstat(current);
      } else continue;
    }
    if (stat.isSymbolicLink() || (parent && !stat.isDirectory()))
      throw new Error('Consumer paths cannot contain symlinks or non-directory parents.');
  }
  return current;
}

/** @param {string} file @param {number} maximum */
export async function boundedFile(file, maximum) {
  const stat = await lstat(file);
  if (!stat.isFile() || stat.isSymbolicLink() || stat.size < 1 || stat.size > maximum)
    throw new Error('Expected a nonempty regular file within the size limit.');
  return stat;
}
