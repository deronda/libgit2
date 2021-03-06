#include <stdio.h>
#include <string.h>

#include <git2.h>

static void check_error(int error_code, const char *action)
{
	if (!error_code)
		return;

	const git_error *error = giterr_last();
	fprintf(stderr, "Error %d %s: %s\n", -error_code, action,
	        (error && error->message) ? error->message : "???");
	exit(1);
}

static int push_commit(git_revwalk *walk, git_object *obj, int hide)
{
	if (hide)
		return git_revwalk_hide(walk, git_object_id(obj));
	else
		return git_revwalk_push(walk, git_object_id(obj));
}

static int push_spec(git_repository *repo, git_revwalk *walk, const char *spec, int hide)
{
	int error;
	git_object *obj;

	if ((error = git_revparse_single(&obj, repo, spec)))
		return error;
	return push_commit(walk, obj, hide);
}

static int push_range(git_repository *repo, git_revwalk *walk, const char *range, int hide)
{
	git_object *left, *right;
	int threedots;
	int error = 0;

	if ((error = git_revparse_rangelike(&left, &right, &threedots, repo, range)))
		return error;
	if (threedots) {
		/* TODO: support "<commit>...<commit>" */
		return GIT_EINVALIDSPEC;
	}

	if ((error = push_commit(walk, left, !hide)))
		goto out;
	error = push_commit(walk, right, hide);

  out:
	git_object_free(left);
	git_object_free(right);
	return error;
}

static int revwalk_parseopts(git_repository *repo, git_revwalk *walk, int nopts, const char *const *opts)
{
	int hide, i, error;
	unsigned int sorting = GIT_SORT_NONE;

	hide = 0;
	for (i = 0; i < nopts; i++) {
		if (!strcmp(opts[i], "--topo-order")) {
			sorting = GIT_SORT_TOPOLOGICAL | (sorting & GIT_SORT_REVERSE);
			git_revwalk_sorting(walk, sorting);
		} else if (!strcmp(opts[i], "--date-order")) {
			sorting = GIT_SORT_TIME | (sorting & GIT_SORT_REVERSE);
			git_revwalk_sorting(walk, sorting);
		} else if (!strcmp(opts[i], "--reverse")) {
			sorting = (sorting & ~GIT_SORT_REVERSE)
			    | ((sorting & GIT_SORT_REVERSE) ? 0 : GIT_SORT_REVERSE);
			git_revwalk_sorting(walk, sorting);
		} else if (!strcmp(opts[i], "--not")) {
			hide = !hide;
		} else if (opts[i][0] == '^') {
			if ((error = push_spec(repo, walk, opts[i] + 1, !hide)))
				return error;
		} else if (strstr(opts[i], "..")) {
			if ((error = push_range(repo, walk, opts[i], hide)))
				return error;
		} else {
			if ((error = push_spec(repo, walk, opts[i], hide)))
				return error;
		}
	}

	return 0;
}

int main (int argc, char **argv)
{
	int error;
	git_repository *repo;
	git_revwalk *walk;
	git_oid oid;
	char buf[41];

	error = git_repository_open_ext(&repo, ".", 0, NULL);
	check_error(error, "opening repository");

	error = git_revwalk_new(&walk, repo);
	check_error(error, "allocating revwalk");
	error = revwalk_parseopts(repo, walk, argc-1, argv+1);
	check_error(error, "parsing options");

	while (!git_revwalk_next(&oid, walk)) {
		git_oid_fmt(buf, &oid);
		buf[40] = '\0';
		printf("%s\n", buf);
	}

	return 0;
}

