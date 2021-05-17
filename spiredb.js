var core_tiers = ["common", "uncommon", "rare", "epic", "legendary", "magnificent", "ethereal"];
var core_mods = {"F":"fire", "P":"poison", "L":"lightning", "S":"strength", "C":"condenser", "R":"runestones"};
var number_suffices = ["", "k", "M", "B", "T", "Qa", "Qi", "Sx", "Sp", "Oc"];

function format_number(num)
{
	var magnitude = 0;
	while(num>1000)
	{
		magnitude += 1;
		num /= 1000;
	}
	if(magnitude>0)
		return num.toFixed(2)+number_suffices[magnitude];
	else
		return num;
}

function SpireClient()
{
	this.spire = document.getElementById("spire");
	this.stats = document.getElementById("stats");
	this.upgrades = document.getElementById("upgrades");
	this.core = document.getElementById("core");
	this.ui = document.getElementById("ui");
	this.query_form = this.ui.querySelector("form");
	this.error_display = this.ui.querySelector(".error");

	this.create_spire = function create_spire(floors, traps)
	{
		this.spire.innerText = "";

		for(var i=floors-1; i>=0; --i)
		{
			var row = document.createElement("tr");
			this.spire.appendChild(row);
			for(var j=0; j<5; ++j)
			{
				var cell = document.createElement("td");
				var trap = traps[i*5+j];
				if(trap)
				{
					cell.innerText = trap;
					cell.classList.add(trap);
				}
				row.appendChild(cell);
			}
		}
	}

	this.query_spire = function query_spire()
	{
		var fe = this.query_form.elements;
		var upgrades = fe.fire.value+fe.frost.value+fe.poison.value+fe.lightning.value;
		var floors = fe.floors.value;
		var budget = fe.budget.value;
		var core = fe.core_tier.value;
		if(fe.core_fire.value>0)
			core += "/F:"+fe.core_fire.value;
		if(fe.core_poison.value>0)
			core += "/P:"+fe.core_poison.value;
		if(fe.core_lightning.value>0)
			core += "/L:"+fe.core_lightning.value;
		if(fe.core_strength.value>0)
			core += "/S:"+fe.core_strength.value;
		if(fe.core_condenser.value>0)
			core += "/C:"+fe.core_condenser.value;
		if(fe.core_runestones.value>0)
			core += "/R:"+fe.core_runestones.value;
		var query = "upg="+upgrades+" f="+floors+" rs="+budget+" core="+core;
		var xhr = new XMLHttpRequest();
		xhr.open("POST", "/query");
		var _this = this;
		xhr.addEventListener("load", function(){ _this.query_complete(xhr); });
		xhr.send(query);
	}

	this.query_complete = function query_complete(xhr)
	{
		var space = xhr.response.indexOf(" ");
		var result = xhr.response.substring(0, space);
		if(result=="ok")
		{
			this.error_display.innerText = "";
			var fields = {};
			var start = 3;
			while(start<xhr.response.length)
			{
				space = xhr.response.indexOf(" ", start);
				if(space<0)
					space = xhr.response.length;
				var equal = xhr.response.indexOf("=", start);
				if(equal<space)
					fields[xhr.response.substring(start, equal)] = xhr.response.substring(equal+1, space);
				start = space+1;
			}

			this.create_spire(fields.t.length/5, fields.t);

			this.stats.style.display = "block";
			this.stats.querySelector("#cost").innerText = format_number(fields.rs);
			this.stats.querySelector("#damage").innerText = format_number(fields.damage);
			this.stats.querySelector("#income_sec").innerText = format_number(fields.income);
			this.stats.querySelector("#income_hr").innerText = format_number(fields.income*3600);
			this.stats.querySelector("#threat").innerText = fields.threat;

			this.upgrades.style.display = "block";
			this.upgrades.querySelector("span").innerText = fields.upg;

			if(fields.core)
			{
				var span = this.core.querySelector("span");
				var core = {};
				var parts = fields.core.split("/");
				span.innerText = core_tiers[parts[0]-1];
				for(var i=1; i<parts.length; ++i)
				{
					span.appendChild(document.createTextNode(" "));
					var mod = document.createElement("span");
					mod.classList.add(parts[i][0]);
					mod.innerText = core_mods[parts[i][0]]+" "+parts[i].substring(2)+"%";
					span.appendChild(mod);
				}

				this.core.style.display = "block";
			}
			else
				this.core.style.display = "none";
		}
		else
		{
			this.error_display.innerText = xhr.response;
		}
	}

	var _this = this;
	this.query_form.addEventListener("submit", function(event){ _this.query_spire(); event.preventDefault(); });

	this.create_spire(5, "");
}

var client = null;

document.addEventListener("DOMContentLoaded", function(){ client = new SpireClient(); });
