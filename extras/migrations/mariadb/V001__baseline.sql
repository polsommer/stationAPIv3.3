CREATE TABLE IF NOT EXISTS avatar (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id BIGINT UNSIGNED NOT NULL,
    name VARCHAR(255) NOT NULL,
    address VARCHAR(255) NOT NULL,
    attributes BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uq_avatar_name_address (name, address)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS room (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    creator_id BIGINT UNSIGNED NOT NULL,
    creator_name VARCHAR(255) NOT NULL,
    creator_address VARCHAR(255) NOT NULL,
    room_name VARCHAR(255) NOT NULL,
    room_topic TEXT NOT NULL,
    room_password VARCHAR(255) NOT NULL,
    room_prefix VARCHAR(255) NOT NULL,
    room_address VARCHAR(255) NOT NULL,
    room_attributes BIGINT UNSIGNED NOT NULL,
    room_max_size BIGINT UNSIGNED NOT NULL,
    room_message_id BIGINT UNSIGNED NOT NULL,
    created_at BIGINT UNSIGNED NOT NULL,
    node_level BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uq_room_name_address (room_name, room_address),
    KEY idx_room_address (room_address),
    CONSTRAINT fk_room_creator
        FOREIGN KEY (creator_id) REFERENCES avatar (id)
        ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS room_administrator (
    administrator_avatar_id BIGINT UNSIGNED NOT NULL,
    room_id BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (administrator_avatar_id, room_id),
    KEY idx_room_admin_room (room_id),
    CONSTRAINT fk_room_admin_avatar
        FOREIGN KEY (administrator_avatar_id) REFERENCES avatar (id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_room_admin_room
        FOREIGN KEY (room_id) REFERENCES room (id)
        ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS room_moderator (
    moderator_avatar_id BIGINT UNSIGNED NOT NULL,
    room_id BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (moderator_avatar_id, room_id),
    KEY idx_room_moderator_room (room_id),
    CONSTRAINT fk_room_moderator_avatar
        FOREIGN KEY (moderator_avatar_id) REFERENCES avatar (id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_room_moderator_room
        FOREIGN KEY (room_id) REFERENCES room (id)
        ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS room_ban (
    banned_avatar_id BIGINT UNSIGNED NOT NULL,
    room_id BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (banned_avatar_id, room_id),
    KEY idx_room_ban_room (room_id),
    CONSTRAINT fk_room_ban_avatar
        FOREIGN KEY (banned_avatar_id) REFERENCES avatar (id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_room_ban_room
        FOREIGN KEY (room_id) REFERENCES room (id)
        ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS room_invite (
    invited_avatar_id BIGINT UNSIGNED NOT NULL,
    room_id BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (invited_avatar_id, room_id),
    KEY idx_room_invite_room (room_id),
    CONSTRAINT fk_room_invite_avatar
        FOREIGN KEY (invited_avatar_id) REFERENCES avatar (id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_room_invite_room
        FOREIGN KEY (room_id) REFERENCES room (id)
        ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS persistent_message (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    avatar_id BIGINT UNSIGNED NOT NULL,
    from_name VARCHAR(255) NOT NULL,
    from_address VARCHAR(255) NOT NULL,
    subject VARCHAR(255) NOT NULL,
    sent_time BIGINT UNSIGNED NOT NULL,
    status INT UNSIGNED NOT NULL,
    folder VARCHAR(255) NOT NULL,
    category VARCHAR(255) NOT NULL,
    message MEDIUMTEXT NOT NULL,
    oob LONGBLOB NULL,
    PRIMARY KEY (id),
    KEY idx_persistent_avatar_status (avatar_id, status),
    KEY idx_persistent_avatar_category (avatar_id, category),
    CONSTRAINT fk_persistent_avatar
        FOREIGN KEY (avatar_id) REFERENCES avatar (id)
        ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS friend (
    avatar_id BIGINT UNSIGNED NOT NULL,
    friend_avatar_id BIGINT UNSIGNED NOT NULL,
    comment VARCHAR(1024) NOT NULL,
    PRIMARY KEY (avatar_id, friend_avatar_id),
    KEY idx_friend_friend_avatar (friend_avatar_id),
    CONSTRAINT fk_friend_avatar
        FOREIGN KEY (avatar_id) REFERENCES avatar (id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_friend_friend_avatar
        FOREIGN KEY (friend_avatar_id) REFERENCES avatar (id)
        ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `ignore` (
    avatar_id BIGINT UNSIGNED NOT NULL,
    ignore_avatar_id BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (avatar_id, ignore_avatar_id),
    KEY idx_ignore_ignore_avatar (ignore_avatar_id),
    CONSTRAINT fk_ignore_avatar
        FOREIGN KEY (avatar_id) REFERENCES avatar (id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_ignore_ignore_avatar
        FOREIGN KEY (ignore_avatar_id) REFERENCES avatar (id)
        ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS schema_version (
    version INT UNSIGNED NOT NULL,
    applied_at BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (version)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO schema_version (version, applied_at) VALUES (1, UNIX_TIMESTAMP());
