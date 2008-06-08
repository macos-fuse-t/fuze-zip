#include <assert.h>

#include "fileNode.h"
#include "fuseZipData.h"

FileNode::FileNode(FuseZipData *_data, const char *fname, int id): data(_data) {
    this->saving = false;
    this->id = id;
    this->is_dir = false;
    this->open_count = 0;
    parse_name(strdup(fname));
    attach();
    this->changed = (id == -2);
    if (this->changed) {
        buffer = new BigBuffer();
        zip_stat_init(&stat);
    } else {
        //TODO: handle error
        if (id != -1) {
            zip_stat_index(data->m_zip, this->id, 0, &stat);
        } else {
            zip_stat_init(&stat);
        }
    }
}

FileNode::~FileNode() {
    free(full_name);
    if (changed && !saving) {
        delete buffer;
    }
}

void FileNode::parse_name(char *fname) {
    assert(fname != NULL);
    this->full_name = fname;
    if (*fname == '\0') {
        // in case of root directory of a virtual filesystem
        this->name = this->full_name;
        this->is_dir = true;
    } else {
        char *lsl = full_name;
        while (*lsl++) {}
        lsl--;
        while (lsl > full_name && *lsl != '/') {
            lsl--;
        }
        // If the last symbol in file name is '/' then it is a directory
        if (*lsl == '/' && *(lsl+1) == '\0') {
            // It will produce two \0s at the end of file name. I think that it is not a problem
            *lsl = '\0';
            this->is_dir = true;
            while (lsl > full_name && *lsl != '/') {
                lsl--;
            }
        }
        // Setting short name of node
        if (*lsl == '/') {
            lsl++;
        }
        this->name = lsl;
    }
}

void FileNode::attach() {
    if (*full_name != '\0') {
        // Adding new child to parent node. For items without '/' in fname it will be root_node.
        char *lsl = name;
        if (lsl > full_name) {
            lsl--;
        }
        char c = *lsl;
        *lsl = '\0';
        // Searching for parent node...
        filemap_t::iterator parent = data->files.find(this->full_name);
        assert(parent != data->files.end());
        parent->second->childs.push_back(this);
        this->parent = parent->second;
        *lsl = c;
    }
    data->files[this->full_name] = this;
}

void FileNode::detach() {
    data->files.erase(full_name);
    parent->childs.remove(this);
}

void FileNode::rename(char *fname) {
    detach();
    free(full_name);
    parse_name(fname);
    attach();
}

void FileNode::rename_wo_reparenting(char *new_name) {
    free(full_name);
    parse_name(new_name);
}

int FileNode::open() {
    if (!changed) {
        buffer = new BigBuffer(data->m_zip, id, stat.size);
    }
    //TODO: error
    return 0;
}

int FileNode::read(char *buf, size_t size, off_t offset) const {
    return buffer->read(buf, size, offset);
}

int FileNode::write(const char *buf, size_t size, off_t offset) {
    changed = true;
    return buffer->write(buf, size, offset);
}

int FileNode::close() {
    stat.size = buffer->len;
    if (!changed) {
        delete buffer;
    }
    return 0;
}

int FileNode::save() {
    int res = buffer->saveToZip(data->m_zip, full_name);
    saving = true;
    return res;
}