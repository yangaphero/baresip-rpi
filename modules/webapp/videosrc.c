#include <re.h>
#include <baresip.h>
#include <string.h>
#include "webapp.h"

static struct odict *videosrcs = NULL;

static char filename[256] = "";


const struct odict* webapp_videosrc_get(void) {
	return (const struct odict *)videosrcs;
}


static int videosrc_register(const struct odict_entry *o)
{
	struct le *le;
	struct pl pl;
	char buf[512] = {0};
	char dev[100] = {0};
	char name[50] = {0};
	//struct contacts *mycontacts = baresip_contacts();

	int err = 0;

	if (o->type == ODICT_OBJECT) {
		le = o->u.odict->lst.head;

	}
	else {
		le = (void *)&o->le;
	}

	for (le=le; le; le=le->next) {
		const struct odict_entry *e = le->data;

		if (e->type != ODICT_STRING) {
			continue;
		}

		if (!str_cmp(e->key, "name")) {
			str_ncpy(name, e->u.str, sizeof(name));
		}
		else if (!str_cmp(e->key, "dev")) {
			str_ncpy(dev, e->u.str, sizeof(dev));
		}
		else if (!str_cmp(e->key, "command")) {
			continue;
		}
		else if (!str_cmp(e->key, "status")) {
			continue;
		}
	}

	re_snprintf(buf, sizeof(buf), "\"%s\"<sip:%s>",
			name, dev);

	pl.p = buf;
	pl.l = strlen(buf);
	//contact_add(mycontacts, NULL, &pl);

	return err;
}


void webapp_videosrc_add(const struct odict_entry *videosrc)
{
	videosrc_register(videosrc);
	webapp_odict_add(videosrcs, videosrc);
	webapp_write_file_json(videosrcs, filename);
}


void webapp_videosrc_delete(const char *dev)
{
	struct le *le;
	for (le = videosrcs->lst.head; le; le = le->next) {
		char o_dev[100];
		const struct odict_entry *o = le->data;
		const struct odict_entry *e;

		e = odict_lookup(o->u.odict, "dev");
		if (!e)
			continue;
		str_ncpy(o_dev, e->u.str, sizeof(o_dev));

		if (!str_cmp(o_dev, dev)) {
			odict_entry_del(videosrcs, o->key);	
			webapp_write_file_json(videosrcs, filename);
			break;
		}
	}
}


int webapp_videosrc_init(void)
{
	char path[256] = "";
	struct mbuf *mb;
	struct le *le;
	int err = 0;

	mb = mbuf_alloc(8192);
	if (!mb)
		return ENOMEM;

	err = conf_path_get(path, sizeof(path));
	if (err)
		goto out;

	if (re_snprintf(filename, sizeof(filename),
				"%s/videosrc.json", path) < 0)
		return ENOMEM;

	err = webapp_load_file(mb, filename);
	if (err) {
		err = odict_alloc(&videosrcs, DICT_BSIZE);
	}
	else {
		err = json_decode_odict(&videosrcs, DICT_BSIZE,
				(char *)mb->buf, mb->end, MAX_LEVELS);
	}
	if (err)
		goto out;

	for (le = videosrcs->lst.head; le; le = le->next) {
		videosrc_register(le->data);
	}

out:
	mem_deref(mb);
	return err;
}


void webapp_videosrc_close(void)
{
	webapp_write_file_json(videosrcs, filename);
	mem_deref(videosrcs);
}
