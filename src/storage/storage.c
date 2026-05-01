#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>
#include <linux/string.h>

#include "storage/storage.h"

static vg_st_storage *vg_storage = NULL;
static DEFINE_SPINLOCK(vg_htable_lock);

static inline void
vg_mm_dump_add_event(vg_mm_dump *dump, vg_mm_event *event) {
    list_add_tail(&event->node, &dump->events);
    dump->nr_events++;
}

static inline void
vg_mm_dump_init(vg_mm_dump *dump, pid_t pid)
{
	dump->pid = pid;
	INIT_LIST_HEAD(&dump->events);
	dump->nr_events = 0;
}

static vg_mm_dump *
vg_st_find_dump_locked(pid_t pid) {
	vg_mm_dump *dump;

	hash_for_each_possible(vg_storage->htable, dump, hnode, pid) {
		if (dump->pid == pid)
			return dump;
	}
	return NULL;
}

extern void
vg_mm_dump_destroy(vg_mm_dump *dump) {
	vg_mm_event *event, *tmp;

	if (!dump)
		return;

	list_for_each_entry_safe(event, tmp, &dump->events, node) {
		list_del(&event->node);
		kfree(event);
	}
	kfree(dump);
}

extern bool
vg_st_create_storage(void) {
	vg_st_storage *new_storage;

	new_storage = kmalloc(sizeof(*new_storage), GFP_KERNEL);
	if (!new_storage)
		return false;
	hash_init(new_storage->htable);

	spin_lock(&vg_htable_lock);
	if (vg_storage) {
		spin_unlock(&vg_htable_lock);
		kfree(new_storage);
		return false;
	}
	vg_storage = new_storage;
	spin_unlock(&vg_htable_lock);

	pr_info("vg_storage: created with %d bits hash\n", VG_ST_HASH_BITS);
	return true;
}

extern void
vg_st_destroy_storage(void) {
	vg_mm_dump *dump;
	struct hlist_node *tmp;
	int bucket;

	spin_lock(&vg_htable_lock);
	if (!vg_storage) {
		spin_unlock(&vg_htable_lock);
		return;
	}

	hash_for_each_safe(vg_storage->htable, bucket, tmp, dump, hnode) {
		hash_del(&dump->hnode);
		vg_mm_dump_destroy(dump);
	}

	kfree(vg_storage);
	vg_storage = NULL;
	spin_unlock(&vg_htable_lock);

	pr_info("vg_storage: destroyed\n");
}

extern bool
vg_st_add_event(pid_t pid, vg_mm_event *event) {
	vg_mm_dump *dump, *new_dump;

	if (!event)
		return false;

	spin_lock(&vg_htable_lock);
	if (!vg_storage) {
		spin_unlock(&vg_htable_lock);
		return false;
	}

	dump = vg_st_find_dump_locked(pid);
	if (dump) {
		vg_mm_dump_add_event(dump, event);
		spin_unlock(&vg_htable_lock);
		return true;
	}

	new_dump = kmalloc(sizeof(*new_dump), GFP_ATOMIC);
	if (!new_dump) {
		spin_unlock(&vg_htable_lock);
		return false;
	}
	vg_mm_dump_init(new_dump, pid);
	vg_mm_dump_add_event(new_dump, event);

	hash_add(vg_storage->htable, &new_dump->hnode, pid);
	spin_unlock(&vg_htable_lock);
	return true;
}

extern void
vg_st_delete_dump(pid_t pid) {
	vg_mm_dump *dump;

	spin_lock(&vg_htable_lock);
	if (!vg_storage) {
		spin_unlock(&vg_htable_lock);
		return;
	}

	dump = vg_st_find_dump_locked(pid);
	if (dump) {
		hash_del(&dump->hnode);
		vg_mm_dump_destroy(dump);
	}
	spin_unlock(&vg_htable_lock);
}

static struct vg_mm_event_s *
vg_mm_event_copy(const struct vg_mm_event_s *event)
{
	struct vg_mm_event_s *new_event;
	size_t size;

	switch (event->type) {
	case VG_MM_EXEC:
		size = sizeof(struct vg_mm_exec_s);
		break;
	default:
		return NULL;
	}

	new_event = kmalloc(size, GFP_ATOMIC);
	if (new_event)
		memcpy(new_event, event, size);
	return new_event;
}

extern struct vg_mm_dump_s *
vg_st_get_dump_copy(pid_t pid)
{
	struct vg_mm_dump_s *dump, *copy = NULL;
	struct vg_mm_event_s *event, *new_event;

	spin_lock(&vg_htable_lock);
	if (!vg_storage)
		goto fail;

	dump = vg_st_find_dump_locked(pid);
	if (!dump)
		goto fail;

	copy = kmalloc(sizeof(*copy), GFP_ATOMIC);
	if (!copy)
		goto fail;

	vg_mm_dump_init(copy, pid);
	copy->nr_events = dump->nr_events;

	list_for_each_entry(event, &dump->events, node) {
		new_event = vg_mm_event_copy(event);
		if (!new_event) {
			vg_mm_dump_destroy(copy);
			copy = NULL;
			goto fail;
		}
		list_add_tail(&new_event->node, &copy->events);
	}

	spin_unlock(&vg_htable_lock);
	return copy;

fail:
	spin_unlock(&vg_htable_lock);
	return NULL;
}
