CREATE TABLE core_mods (
    id integer NOT NULL,
    core_id integer NOT NULL,
    mod smallint NOT NULL,
    value integer NOT NULL
);

CREATE SEQUENCE core_mods_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;
ALTER SEQUENCE core_mods_id_seq OWNED BY core_mods.id;

CREATE TABLE cores (
    id integer NOT NULL,
    tier smallint NOT NULL,
    type character varying(4) NOT NULL,
    cost bigint NOT NULL,
    version integer DEFAULT 0 NOT NULL
);

CREATE SEQUENCE cores_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;
ALTER SEQUENCE cores_id_seq OWNED BY cores.id;

CREATE TABLE layouts (
    id integer NOT NULL,
    floors smallint NOT NULL,
    fire smallint NOT NULL,
    frost smallint NOT NULL,
    poison smallint NOT NULL,
    lightning smallint NOT NULL,
    traps text NOT NULL,
    damage bigint NOT NULL,
    threat smallint NOT NULL,
    rs_per_sec bigint NOT NULL,
    towers smallint NOT NULL,
    cost numeric(40,0) NOT NULL,
    submitter text NOT NULL,
    version integer DEFAULT 0 NOT NULL,
    core_id integer
);

CREATE SEQUENCE layouts_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;
ALTER SEQUENCE layouts_id_seq OWNED BY layouts.id;

ALTER TABLE ONLY core_mods ALTER COLUMN id SET DEFAULT nextval('core_mods_id_seq'::regclass);
ALTER TABLE ONLY cores ALTER COLUMN id SET DEFAULT nextval('cores_id_seq'::regclass);
ALTER TABLE ONLY layouts ALTER COLUMN id SET DEFAULT nextval('layouts_id_seq'::regclass);

ALTER TABLE ONLY cores
    ADD CONSTRAINT cores_pkey PRIMARY KEY (id);

CREATE UNIQUE INDEX core_mods_id_idx ON core_mods USING btree (id);
CREATE INDEX core_mods_layout_id_idx ON core_mods USING btree (core_id);
CREATE INDEX layouts_damage_idx ON layouts USING btree (damage);
CREATE INDEX layouts_floors_cost_idx ON layouts USING btree (floors, cost);
CREATE UNIQUE INDEX layouts_id_idx ON layouts USING btree (id);

ALTER TABLE ONLY core_mods
    ADD CONSTRAINT core_mods_core_id_fkey FOREIGN KEY (core_id) REFERENCES cores(id) ON DELETE CASCADE;

ALTER TABLE ONLY layouts
    ADD CONSTRAINT layouts_core_id_fkey FOREIGN KEY (core_id) REFERENCES cores(id);
